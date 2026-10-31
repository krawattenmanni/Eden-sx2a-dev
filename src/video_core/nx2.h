// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <mutex>
#include <vector>

#include "common/common_types.h"

namespace Tegra {

// Set once a Switch 2 (Ampere, NVN2) driver is talking to the GPU. Its command streams use
// Ampere classes and the Turing+ macro ISA, which Eden's Maxwell engines don't implement.
inline std::atomic<bool> g_nx2_ampere{false};

/// Parses a switch that is on unless NAME=0. Callers cache the result and also check
/// g_nx2_ampere, so Switch 1 programs never see it on.
[[nodiscard]] inline bool Nx2EnvDefaultOn(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr || value[0] != '0';
}

// Device pages the GPU has written through the buffer cache (shader stores, copies), ever. Donkey
// Kong Bananza's culling pass fills in words its pushbuffer then executes (indirect draw records
// consumed as macro parameters); read without a flush they hold older words. The bits never
// clear: the work that rewrites a page may still be queued on the split's back when its front
// reads the command list. One bit per 4 KiB of the 36-bit device address space (2 MiB).
inline constexpr u64 Nx2WrittenPageBits = 12;
inline std::array<std::atomic<u64>, (u64{1} << (36 - Nx2WrittenPageBits)) / 64> g_nx2_gpu_written_pages{};

enum class Nx2WriteSource : u32 { Shader, Copy };
/// Pages newly marked by each source, for EDEN_NX2_PBFLUSH_STATS.
inline std::array<std::atomic<u64>, 2> g_nx2_written_pages_marked{};

inline void Nx2MarkGpuWritten(u64 device_address, u64 size, Nx2WriteSource source) {
    if (size == 0) {
        return;
    }
    const u64 last = (std::min)((device_address + size - 1) >> Nx2WrittenPageBits,
                                u64{g_nx2_gpu_written_pages.size()} * 64 - 1);
    u64 marked = 0;
    for (u64 page = device_address >> Nx2WrittenPageBits; page <= last; ++page) {
        auto& word = g_nx2_gpu_written_pages[page / 64];
        const u64 bit = u64{1} << (page % 64);
        if ((word.load(std::memory_order_relaxed) & bit) == 0) {
            word.fetch_or(bit, std::memory_order_relaxed);
            ++marked;
        }
    }
    if (marked != 0) {
        g_nx2_written_pages_marked[static_cast<u32>(source)].fetch_add(marked, std::memory_order_relaxed);
    }
}

/// Whether buffer copies mark pages too (EDEN_NX2_PBFLUSH_COPY_MARK=0: only shader stores do).
[[nodiscard]] inline bool Nx2MarkCopies() {
    static const bool mark = [] {
        const char* value = std::getenv("EDEN_NX2_PBFLUSH_COPY_MARK");
        return value == nullptr || value[0] != '0';
    }();
    return mark;
}

/// A new emulation session starts: no page has been written by its GPU yet.
inline void Nx2ClearGpuWrittenPages() {
    for (auto& word : g_nx2_gpu_written_pages) {
        word.store(0, std::memory_order_relaxed);
    }
}

[[nodiscard]] inline bool Nx2IsGpuWritten(u64 device_address) {
    const u64 page = device_address >> Nx2WrittenPageBits;
    if (page >= u64{g_nx2_gpu_written_pages.size()} * 64) {
        return false;
    }
    return (g_nx2_gpu_written_pages[page / 64].load(std::memory_order_relaxed) >> (page % 64)) & 1;
}

// NVN2 queues release their progress values into the second page of a two-page GPU-accessible
// region and GPU acquires read the same offsets in the first page (the CPU polls the second).
// Something on the console copies one into the other; until that is known, the puller mirrors
// releases that land in a second page. Every queue has such a region, so keep a small table.
inline constexpr size_t Nx2MaxSemaphoreRegions = 32;
inline std::array<std::atomic<u64>, Nx2MaxSemaphoreRegions> g_nx2_semaphore_regions{}; // starts

inline void Nx2AddSemaphoreRegion(u64 start) {
    for (auto& region : g_nx2_semaphore_regions) {
        if (region.load() == start) {
            return;
        }
    }
    for (auto& region : g_nx2_semaphore_regions) {
        u64 expected = 0;
        if (region.compare_exchange_strong(expected, start)) {
            return;
        }
    }
}

/// The region's memory was destroyed: stop mirroring into it.
inline void Nx2RemoveSemaphoreRegion(u64 start) {
    for (auto& region : g_nx2_semaphore_regions) {
        u64 expected = start;
        region.compare_exchange_strong(expected, 0);
    }
}

/// GPU VA in the first page that mirrors a release at `address`, or 0.
inline u64 Nx2SemaphoreMirror(u64 address) {
    for (const auto& region : g_nx2_semaphore_regions) {
        const u64 start = region.load(std::memory_order_relaxed);
        if (start == 0) {
            continue;
        }
        if (address >= start + 0x1000 && address < start + 0x2000) {
            return address - 0x1000;
        }
    }
    return 0;
}

/// Recent semaphore releases from every path, for diagnosing acquires that never resume.
enum class Nx2ReleaseSource : u32 { Puller = 1, PullerMirror, QmdAdd, QmdMax, QmdWrite, Report3D };
struct Nx2ReleaseRecord {
    std::atomic<u64> address;
    std::atomic<u32> value;
    std::atomic<u32> source;
};
inline std::array<Nx2ReleaseRecord, 512> g_nx2_release_log{};
inline std::atomic<u32> g_nx2_release_index{0};

inline void Nx2RecordRelease(u64 address, u32 value, Nx2ReleaseSource source) {
    auto& record = g_nx2_release_log[g_nx2_release_index.fetch_add(1, std::memory_order_relaxed) %
                                     g_nx2_release_log.size()];
    record.address.store(address, std::memory_order_relaxed);
    record.value.store(value, std::memory_order_relaxed);
    record.source.store(static_cast<u32>(source), std::memory_order_relaxed);
}

/// Channels suspended on a semaphore acquire. A console channel polls and sees every release;
/// Eden writes releases in bursts when fences signal and resumes waiters only between command
/// lists, so an acquire-equal could miss its value. Writes latch the waiters they satisfy.
class Nx2SemaphoreWaiters {
public:
    // Puller GpuSemaphoreOperation values.
    static constexpr u32 AcquireEqual = 0x1;
    static constexpr u32 AcquireGequal = 0x4;
    static constexpr u32 AcquireMask = 0x8;

    static Nx2SemaphoreWaiters& Get() {
        static Nx2SemaphoreWaiters waiters;
        return waiters;
    }

    void Add(const void* owner, u64 device_address, u32 value, u32 operation) {
        std::scoped_lock lk{mutex};
        std::erase_if(waiters, [owner](const Waiter& w) { return w.owner == owner; });
        waiters.push_back({owner, device_address, value, operation, false});
        count.store(waiters.size(), std::memory_order_release);
    }

    void Remove(const void* owner) {
        if (count.load(std::memory_order_acquire) == 0) {
            return;
        }
        std::scoped_lock lk{mutex};
        std::erase_if(waiters, [owner](const Waiter& w) { return w.owner == owner; });
        count.store(waiters.size(), std::memory_order_release);
    }

    [[nodiscard]] bool Latched(const void* owner) {
        if (count.load(std::memory_order_acquire) == 0) {
            return false;
        }
        std::scoped_lock lk{mutex};
        return std::ranges::any_of(waiters,
                                   [owner](const Waiter& w) { return w.owner == owner && w.latched; });
    }

    /// A GPU write of `value` to the u32 at `device_address`.
    void OnWrite(u64 device_address, u32 value) {
        if (count.load(std::memory_order_acquire) == 0) {
            return;
        }
        std::scoped_lock lk{mutex};
        for (auto& w : waiters) {
            if (w.address == device_address && !w.latched && Matches(w, value)) {
                w.latched = true;
            }
        }
    }

private:
    struct Waiter {
        const void* owner;
        u64 address;
        u32 value;
        u32 operation;
        bool latched;
    };

    static bool Matches(const Waiter& w, u32 current) {
        switch (w.operation) {
        case AcquireEqual:
            return current == w.value;
        case AcquireGequal:
            return current >= w.value;
        case AcquireMask:
            return (current & w.value) != 0;
        default:
            return false;
        }
    }

    std::mutex mutex;
    std::vector<Waiter> waiters;
    std::atomic<size_t> count{};
};

} // namespace Tegra
