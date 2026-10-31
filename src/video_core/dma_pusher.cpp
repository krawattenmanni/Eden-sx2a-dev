// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <string>

#include "common/settings.h"
#include "core/core.h"
#include "core/memory.h"
#include "video_core/nx2_profile.h"
#include "video_core/dma_pusher.h"
#include "video_core/control/channel_state.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/gpu.h"
#include "video_core/gpu_split.h"
#include "video_core/guest_memory.h"
#include "video_core/memory_manager.h"
#include "video_core/nx2.h"
#include "video_core/rasterizer_interface.h"

namespace Tegra {

constexpr u32 MacroRegistersStart = 0xE00;
constexpr u32 ComputeInline = 0x6D;

namespace {
// EDEN_NX2_PBFLUSH_STATS=1: what the pushbuffer flush checks and what it costs, every 5 s.
struct PbFlushStats {
    std::atomic<u64> checked{};
    std::array<std::atomic<u64>, 3> hits{}; ///< Entries on a GPU-written page: <=8, <=64, >64 words
    std::atomic<u64> skipped{};             ///< Hits over EDEN_NX2_PBFLUSH_MAX, not flushed
    std::atomic<u64> deferred{};            ///< Data-only entries handed to the split's back
    std::atomic<u64> fresh{};               ///< Hits whose pages were flushed since the last writer
    std::atomic<u64> drains{};
    std::atomic<u64> flushes{};
    std::atomic<u64> differing_words{};
};
PbFlushStats g_pbflush_stats;
// Read on first use: Android sets the switches after the library is loaded.
bool PbFlushStatsEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("EDEN_NX2_PBFLUSH_STATS");
        return value != nullptr && value[0] != '0';
    }();
    return enabled;
}
// Largest entry flushed, in words. The culling pass's draw records are 5-word entries; command
// lists the CPU wrote are larger and their snapshot is right.
size_t PbFlushMaxWords() {
    static const size_t words = [] {
        const char* value = std::getenv("EDEN_NX2_PBFLUSH_MAX");
        return value != nullptr ? static_cast<size_t>(std::strtoull(value, nullptr, 0)) : size_t{64};
    }();
    return words;
}

// EDEN_NX2_PBFLUSH_EPOCH (on by default for Switch 2 programs, =0 off): an entry on pages flushed
// (with a drain) since the front last sent work to an engine other than 3D is read without another
// drain. Assumes the words are written by compute or copies, not by 3D draws.
bool PbFlushEpochEnabled() {
    static const bool enabled = Nx2EnvDefaultOn("EDEN_NX2_PBFLUSH_EPOCH");
    return enabled && g_nx2_ampere.load(std::memory_order_relaxed);
}
// Front thread only. Bumped when work that may write GPU memory is sent.
u64 g_writer_epoch = 1;
struct FreshPage {
    u64 device_page;
    u64 epoch;
};
std::array<FreshPage, 256> g_fresh_pages{};

FreshPage& FreshSlot(u64 device_page) {
    return g_fresh_pages[device_page % g_fresh_pages.size()];
}

// A data-only entry whose consumer runs on the split's back: flushed, re-read and executed there.
struct DeferredEntry {
    static constexpr size_t MaxWords = 64;
    Core::System* system;
    MemoryManager* memory;
    Engines::EngineInterface* engine;
    GPUVAddr address;
    u32 words;
    u32 method;
    u32 method_count;
    bool non_incrementing;
    bool increment_once;
    bool wants_segment;
    bool payload_dirty; ///< Refresh the engine's current_dirty after the flush
    std::array<u32, MaxWords> data;
};

void RunDeferredEntry(const DeferredEntry& entry) {
    MemoryManager& memory = *entry.memory;
    const size_t bytes = size_t{entry.words} * sizeof(u32);
    if (memory.IsMemoryDirty(entry.address, bytes, VideoCommon::CacheType::BufferCache)) {
        memory.FlushRegion(entry.address, bytes, VideoCommon::CacheType::BufferCache);
        g_pbflush_stats.flushes.fetch_add(1, std::memory_order_relaxed);
    }
    std::array<u32, DeferredEntry::MaxWords> words = entry.data;
    const GPUVAddr end = entry.address + bytes;
    for (GPUVAddr page = entry.address & ~GPUVAddr{0xfff}; page < end; page += 0x1000) {
        const auto device = memory.GpuToCpuAddress(page);
        if (!device || !Nx2IsGpuWritten(*device)) {
            continue;
        }
        const GPUVAddr begin = (std::max)(page, entry.address);
        const GPUVAddr stop = (std::min)(page + 0x1000, end);
        memory.ReadBlockUnsafe(begin, words.data() + (begin - entry.address) / sizeof(u32), stop - begin);
    }
    if (PbFlushStatsEnabled()) {
        size_t changed = 0;
        for (u32 i = 0; i < entry.words; ++i) {
            changed += words[i] != entry.data[i] ? 1 : 0;
        }
        g_pbflush_stats.differing_words.fetch_add(changed, std::memory_order_relaxed);
    }
    Engines::EngineInterface& engine = *entry.engine;
    Core::System& system = *entry.system;
    if (entry.payload_dirty) {
        engine.current_dirty = memory.IsMemoryDirty(entry.address, bytes);
    }
    // What GpuSplit::Execute does with the Method and Multi entries ProcessCommands would send.
    if (entry.non_incrementing) {
        engine.ConsumeSink(system);
        if (entry.wants_segment) {
            engine.current_dma_segment = entry.address;
        }
        engine.CallMultiMethod(system, entry.method, words.data(), entry.words, entry.method_count);
        return;
    }
    u32 method = entry.method;
    bool non_incrementing = false;
    for (u32 i = 0; i < entry.words; ++i) {
        if (!engine.execution_mask[method]) {
            engine.method_sink.emplace_back(method, words[i]);
        } else {
            engine.ConsumeSink(system);
            if (entry.wants_segment) {
                engine.current_dma_segment = entry.address + i * sizeof(u32);
            }
            engine.CallMethod(system, method, words[i], entry.method_count - i <= 1);
        }
        method += non_incrementing ? 0 : 1;
        non_incrementing |= entry.increment_once;
    }
}

void ReportPbFlushStats() {
    using Clock = std::chrono::steady_clock;
    static Clock::time_point last = Clock::now();
    const auto now = Clock::now();
    if (now - last < std::chrono::seconds{5}) {
        return;
    }
    last = now;
    auto& s = g_pbflush_stats;
    const auto take = [](std::atomic<u64>& counter) { return counter.exchange(0, std::memory_order_relaxed); };
    LOG_INFO(HW_GPU, "NX2 pbflush stats: checked {} hits <=8 {} <=64 {} >64 {} skipped {} deferred {} fresh {} drains {} flushes {} differing words {} (max {} words; new marked pages shader {} copy {})",
             take(s.checked), take(s.hits[0]), take(s.hits[1]), take(s.hits[2]), take(s.skipped),
             take(s.deferred), take(s.fresh), take(s.drains), take(s.flushes),
             take(s.differing_words), PbFlushMaxWords(),
             take(g_nx2_written_pages_marked[0]), take(g_nx2_written_pages_marked[1]));
}
} // Anonymous namespace

DmaPusher::DmaPusher(Core::System& system_, MemoryManager& memory_manager_, Control::ChannelState& channel_state_)
    : system{system_}
    , memory_manager{memory_manager_}
    , channel_state{channel_state_}
    , signal_sync{false}
    , synced{false}
{}

DmaPusher::~DmaPusher() = default;

void DmaPusher::DispatchCalls() {
    // The sub-index is kept: a channel suspended on a semaphore resumes inside its command list.
    dma_state.is_last_call = true;
    while (system.IsPoweredOn()) {
        if (!Step()) {
            break;
        }
    }
    if (auto& split = VideoCommon::GpuSplit::Get(); split.Active()) {
        split.Defer([gpu = &system.GPU()] {
            gpu->FlushCommands();
            gpu->OnCommandListEnd();
        });
        split.Publish();
        return;
    }
    system.GPU().FlushCommands();
    system.GPU().OnCommandListEnd();
}

bool DmaPusher::Step() {
    if (!ib_enable || dma_pushbuffer.empty()) {
        return false;
    }
    if (!puller.ResumeAcquire(*this)) {
        return false;
    }

    CommandList& command_list = dma_pushbuffer.front();

    const size_t prefetch_size = command_list.prefetch_command_list.size();
    const size_t command_list_size = command_list.command_lists.size();

    if (prefetch_size == 0 && command_list_size == 0) {
        dma_pushbuffer.pop();
        dma_pushbuffer_subindex = 0;
        return true;
    }

    if (prefetch_size > 0) {
        const std::span<const CommandHeader> commands{command_list.prefetch_command_list};
        command_resume_offset +=
            ProcessCommands(commands.subspan(command_resume_offset), command_resume_offset);
        if (puller.HasPendingAcquire()) {
            return false;
        }
        command_resume_offset = 0;
        dma_pushbuffer.pop();
        return true;
    }

    auto& current_command = command_list.command_lists[dma_pushbuffer_subindex];
    const CommandListHeader& header = current_command;
    dma_state.dma_get = header.addr;

    if (signal_sync && !synced) {
        std::unique_lock lk(sync_mutex);
        sync_cv.wait(lk, [this]() { return synced; });
        signal_sync = false;
        synced = false;
    }

    if (header.size > 0) {
        // A resumed entry is read again: the words after the acquire may be new by now.
        const bool use_safe = Settings::IsDMALevelDefault() ? Settings::IsGPULevelHigh() : Settings::IsDMALevelSafe();
        const bool has_snapshot = dma_pushbuffer_subindex < command_list.submit_words.size();
        // Words a culling pass wrote (indirect draw records consumed as macro parameters) may
        // only be in host buffers. Safe reads flush on their own; snapshots and unsafe reads
        // flush pages the GPU has written. EDEN_NX2_PBFLUSH=0 turns this off.
        static const bool flush_commands = [] {
            const char* value = std::getenv("EDEN_NX2_PBFLUSH");
            return value == nullptr || value[0] != '0';
        }();
        const WrittenPages written_pages = flush_commands && (has_snapshot || !use_safe)
                                               ? CheckGpuWrittenPages(dma_state.dma_get, header.size)
                                               : WrittenPages::None;
        if (subchannels[dma_state.subchannel] && dma_state.method_count) {
            const auto engine = subchannel_type[dma_state.subchannel];
            const bool kepler_payload = engine == Engines::EngineTypes::KeplerCompute && dma_state.method == ComputeInline && dma_state.non_incrementing;
            const bool macro_payload = engine == Engines::EngineTypes::Maxwell3D && dma_state.method >= MacroRegistersStart;
            if (kepler_payload || macro_payload) {
                const size_t words = std::min<size_t>(dma_state.method_count, header.size);
                auto* const engine_state = subchannels[dma_state.subchannel];
                if (auto& split = VideoCommon::GpuSplit::Get(); split.Active()) {
                    // The caches behind IsMemoryDirty belong to the split's back thread.
                    split.Defer([engine_state, memory = &memory_manager, address = GPUVAddr{dma_state.dma_get},
                                 bytes = words * sizeof(u32)] {
                        engine_state->current_dirty = memory->IsMemoryDirty(address, bytes);
                    });
                } else {
                    engine_state->current_dirty = memory_manager.IsMemoryDirty(dma_state.dma_get, words * sizeof(u32));
                }
            }
        }
        if (command_resume_offset == 0) {
            // A method's data continues across GP entries, whatever their level: title 4 ends a
            // main entry after a macro's first word and supplies the remaining parameters in a
            // 5-word subroutine entry. EDEN_NX2_PARK_SPLIT_METHODS restores the old guess (an
            // entry elsewhere is a call that starts clean; resume where the split one ended).
            static const bool park_split_methods = std::getenv("EDEN_NX2_PARK_SPLIT_METHODS") != nullptr;
            if (g_nx2_ampere) {
                if (static u32 split_logs{}; dma_state.method_count != 0 && split_logs < 64) {
                    ++split_logs;
                    LOG_DEBUG(HW_GPU, "Split method {:#x} ({} left) on ch {}: next entry {:#x} size {} raw {:#x} (level bit {}, priv bit {}), expected {:#x}, parked {}",
                             dma_state.method, dma_state.method_count, channel_state.bind_id,
                             u64{header.addr}, u64{header.size}, header.raw, u64{header.is_push_buffer},
                             u64{header.allow_flush}, split_method_end, parked_methods.size());
                    std::array<u32, 8> next_words{};
                    std::array<u32, 4> prev_words{};
                    memory_manager.ReadBlockUnsafe(header.addr, next_words.data(),
                                             std::min<size_t>(header.size, 8) * sizeof(u32));
                    memory_manager.ReadBlockUnsafe(split_method_end - sizeof(prev_words), prev_words.data(),
                                             sizeof(prev_words));
                    LOG_DEBUG(HW_GPU, "  previous entry ends {:08x}; next entry starts {:08x}",
                              fmt::join(prev_words, " "), fmt::join(next_words, " "));
                }
                if (park_split_methods) {
                    ResolveSplitMethod(header.addr);
                }
            }
            if (dma_pushbuffer_subindex < command_list.submit_hashes.size()) {
                CheckSubmitHash(header, command_list.submit_hashes[dma_pushbuffer_subindex]);
            }
            if (dma_pushbuffer_subindex < command_list.submit_words.size()) {
                CheckSubmitWords(header, command_list.submit_words[dma_pushbuffer_subindex]);
            }
        }
        bool gpu_written = written_pages == WrittenPages::Fresh;
        bool deferred = false;
        if (written_pages == WrittenPages::Stale) {
            const std::span<const u32> snapshot = has_snapshot
                ? std::span<const u32>{command_list.submit_words[dma_pushbuffer_subindex]}
                : std::span<const u32>{};
            deferred = command_resume_offset == 0 && DeferGpuWrittenEntry(header, snapshot);
            gpu_written = !deferred && FlushGpuWrittenCommands(dma_state.dma_get, header.size);
        }
        if (deferred) {
            // Executed on the split's back.
        } else if (has_snapshot) {
            std::span<const u32> words{command_list.submit_words[dma_pushbuffer_subindex]};
            if (gpu_written) {
                // The snapshot predates the GPU work that writes these words: take the pages it
                // wrote from guest memory, now flushed, and the rest from the snapshot.
                gpu_written_words.assign(words.begin(), words.end());
                ReadGpuWrittenWords(header.addr, gpu_written_words);
                if (static std::atomic<u32> reported{};
                    PbFlushStatsEnabled() || reported.load(std::memory_order_relaxed) < 5) {
                    size_t changed = 0;
                    for (size_t i = 0; i < words.size(); ++i) {
                        changed += words[i] != gpu_written_words[i] ? 1 : 0;
                    }
                    g_pbflush_stats.differing_words.fetch_add(changed, std::memory_order_relaxed);
                    if (changed != 0 && reported.fetch_add(1) < 5) {
                        LOG_INFO(HW_GPU, "NX2 pbflush: pb copy entry {:#x}+{:#x} on ch {}: {} words on GPU-written pages differ from the submit snapshot",
                                 header.addr.Value(), header.size.Value(), channel_state.bind_id, changed);
                    }
                }
                words = gpu_written_words;
            }
            const std::span<const CommandHeader> commands{
                reinterpret_cast<const CommandHeader*>(words.data()), words.size()};
            RecordParsedEntry(header, commands);
            command_resume_offset +=
                ProcessCommands(commands.subspan(command_resume_offset), command_resume_offset);
        } else if (use_safe) {
            Tegra::Memory::GpuGuestMemory<Tegra::CommandHeader, Tegra::Memory::GuestMemoryFlags::SafeRead>headers(memory_manager, dma_state.dma_get, header.size, &command_headers);
            const std::span<const CommandHeader> commands{headers.data(), headers.size()};
            RecordParsedEntry(header, commands);
            command_resume_offset +=
                ProcessCommands(commands.subspan(command_resume_offset), command_resume_offset);
        } else {
            Tegra::Memory::GpuGuestMemory<Tegra::CommandHeader, Tegra::Memory::GuestMemoryFlags::UnsafeRead>headers(memory_manager, dma_state.dma_get, header.size, &command_headers);
            const std::span<const CommandHeader> commands{headers.data(), headers.size()};
            RecordParsedEntry(header, commands);
            command_resume_offset +=
                ProcessCommands(commands.subspan(command_resume_offset), command_resume_offset);
        }
        HandleEntryDumpRequest();
        if (puller.HasPendingAcquire()) {
            entry_suspended = true;
            return false;
        }
        command_resume_offset = 0;
        previous_entry_suspended = entry_suspended;
        entry_suspended = false;
        if (dma_state.method_count != 0) {
            split_method_end = header.addr + header.size * sizeof(u32);
        }
    }

    if (++dma_pushbuffer_subindex >= command_list_size) {
        // A split method resumes within the submission that split it; ring addresses repeat
        // every frame, so a stale one could otherwise swallow the headers of a later entry.
        for (const auto& parked : parked_methods) {
            static std::atomic<u32> reported{};
            if (reported.fetch_add(1) < 16) {
                LOG_WARNING(HW_GPU,
                            "Submission ended with parked method {:#x} (sub {}, {} words left, "
                            "resume {:#x})",
                            parked.method, parked.subchannel, parked.method_count,
                            parked.resume_address);
            }
        }
        parked_methods.clear();
        dma_pushbuffer.pop();
        dma_pushbuffer_subindex = 0;
    } else {
        signal_sync = command_list.command_lists[dma_pushbuffer_subindex].sync && Settings::values.sync_memory_operations.GetValue();
    }

    if (signal_sync) {
        VideoCommon::GpuSplit::Get().Drain();
        rasterizer->SignalFence([this]() {
            std::scoped_lock lk(sync_mutex);
            synced = true;
            sync_cv.notify_all();
        });
    }

    return true;
}

size_t DmaPusher::ProcessCommands(std::span<const CommandHeader> commands, size_t word_offset) {
    for (size_t index = 0; index < commands.size();) {
        // Data word of methods command
        if (dma_state.method_count && dma_state.non_incrementing) {
            auto const& command_header = commands[index]; //must ref (MUltiMethod re)
            dma_state.dma_word_offset = u64((word_offset + index) * sizeof(u32));
            u32 max_write = u32(std::min<std::size_t>(index + dma_state.method_count, commands.size()) - index);
            if (g_nx2_ampere && dma_state.method < non_puller_methods) {
                max_write = 1; // a semaphore acquire may suspend the channel after any word
            }
            CallMultiMethod(&command_header.argument, max_write);
            dma_state.method_count -= max_write;
            dma_state.is_last_call = true;
            index += max_write;
        } else if (dma_state.method_count) {
            auto const command_header = commands[index]; //can copy
            dma_state.dma_word_offset = u64((word_offset + index) * sizeof(u32));
            dma_state.is_last_call = dma_state.method_count <= 1;
            CallMethod(command_header.argument);
            dma_state.method += !dma_state.non_incrementing ? 1 : 0;
            dma_state.non_incrementing |= dma_increment_once;
            dma_state.method_count--;
            index++;
        } else {
            auto const command_header = commands[index]; //can copy
            // No command active - this is the first word of a new one
            switch (command_header.mode) {
            case SubmissionMode::Increasing:
                SetState(command_header);
                dma_state.non_incrementing = false;
                dma_increment_once = false;
                break;
            case SubmissionMode::NonIncreasing:
                SetState(command_header);
                dma_state.non_incrementing = true;
                dma_increment_once = false;
                break;
            case SubmissionMode::Inline:
                dma_state.method = command_header.method;
                dma_state.subchannel = command_header.subchannel;
                dma_state.dma_word_offset = u64(-s64(dma_state.dma_get)); // negate to set address as 0
                CallMethod(command_header.arg_count);
                dma_state.non_incrementing = true;
                dma_increment_once = false;
                break;
            case SubmissionMode::IncreaseOnce:
                SetState(command_header);
                dma_state.non_incrementing = false;
                dma_increment_once = true;
                break;
            default:
                break;
            }
            index++;
        }
        if (puller.HasPendingAcquire()) {
            return index;
        }
    }
    return commands.size();
}

// NVN on Switch 2 ends GPFIFO entries in the middle of a method's data and calls another command
// buffer (e.g. a compute dispatch) before the entry that carries the rest, which starts right
// where the first one ended. An entry starting anywhere else is such a call: run it with a clean
// method state and resume the unfinished method when the stream continues at that address.
void DmaPusher::ResolveSplitMethod(GPUVAddr entry_address) {
    const auto continued = std::ranges::find(parked_methods, entry_address,
                                             &ParkedMethod::resume_address);
    std::optional<ParkedMethod> resume;
    if (continued != parked_methods.end()) {
        resume = *continued;
        parked_methods.erase(continued);
    }
    if (dma_state.method_count != 0 && entry_address != split_method_end) {
        if (parked_methods.size() == MaxParkedMethods) {
            static std::atomic<u32> reported{};
            if (reported.fetch_add(1) < 8) {
                const auto& oldest = parked_methods.front();
                LOG_WARNING(HW_GPU,
                            "Dropping parked method {:#x} (sub {}, {} words left, resume {:#x})",
                            oldest.method, oldest.subchannel, oldest.method_count,
                            oldest.resume_address);
            }
            parked_methods.erase(parked_methods.begin());
        }
        parked_methods.push_back(ParkedMethod{
            .method = dma_state.method,
            .subchannel = dma_state.subchannel,
            .method_count = dma_state.method_count,
            .non_incrementing = dma_state.non_incrementing,
            .increment_once = dma_increment_once,
            .resume_address = split_method_end,
        });
        dma_state.method_count = 0;
        dma_increment_once = false;
    }
    if (resume && dma_state.method_count == 0) {
        dma_state.method = resume->method;
        dma_state.subchannel = resume->subchannel;
        dma_state.method_count = resume->method_count;
        dma_state.non_incrementing = resume->non_incrementing;
        dma_increment_once = resume->increment_once;
    }
}

void DmaPusher::SetState(const CommandHeader& command_header) {
    dma_state.method = command_header.method;
    dma_state.subchannel = command_header.subchannel;
    dma_state.method_count = command_header.method_count;
}

// EDEN_NX2_SEM_WATCH=<gpu address>: log every method whose argument is the low word of an
// address in the 16 bytes there (finds the engine and method that release a semaphore).
static const u32 g_semaphore_watch = [] {
    const char* value = std::getenv("EDEN_NX2_SEM_WATCH");
    return value ? static_cast<u32>(std::strtoull(value, nullptr, 16)) : 0U;
}();

// EDEN_NX2_PB_CHECK also keeps the last parsed entries, for DumpCurrentEntry.
static const bool g_record_parsed_entries = std::getenv("EDEN_NX2_PB_CHECK") != nullptr;

void DmaPusher::WatchArgument(u32 method, u32 argument) const {
    const u32 watch = g_semaphore_watch;
    if ((argument & ~0xfU) == watch) {
        LOG_INFO(HW_GPU, "Semaphore watch: channel {} subchannel {} method {:#x} argument {:#x}",
                 channel_state.bind_id, dma_state.subchannel, method, argument);
    }
}

bool DmaPusher::WantsSegment() const {
    // Only macro parameters and compute uploads read the engine's current_dma_segment.
    const auto engine = subchannel_type[dma_state.subchannel];
    return engine == Engines::EngineTypes::KeplerCompute ||
           (engine == Engines::EngineTypes::Maxwell3D && dma_state.method >= MacroRegistersStart);
}

DmaPusher::WrittenPages DmaPusher::CheckGpuWrittenPages(GPUVAddr address, size_t words) const {
    if (!g_nx2_ampere.load(std::memory_order_relaxed) || words == 0) {
        return WrittenPages::None;
    }
    if (PbFlushStatsEnabled()) {
        ReportPbFlushStats();
        g_pbflush_stats.checked.fetch_add(1, std::memory_order_relaxed);
    } else if (words > PbFlushMaxWords()) {
        return WrittenPages::None;
    }
    const bool epoch = PbFlushEpochEnabled();
    bool written = false;
    bool stale = false;
    const GPUVAddr end = address + words * sizeof(u32);
    for (GPUVAddr page = address & ~GPUVAddr{0xfff}; page < end; page += 0x1000) {
        const auto device = memory_manager.GpuToCpuAddress(page);
        if (!device || !Nx2IsGpuWritten(*device)) {
            continue;
        }
        written = true;
        const u64 device_page = *device >> Nx2WrittenPageBits;
        const FreshPage& slot = FreshSlot(device_page);
        if (!epoch || slot.device_page != device_page || slot.epoch != g_writer_epoch) {
            stale = true;
            break;
        }
    }
    if (!written) {
        return WrittenPages::None;
    }
    if (PbFlushStatsEnabled()) {
        g_pbflush_stats.hits[words <= 8 ? 0 : words <= 64 ? 1 : 2].fetch_add(1, std::memory_order_relaxed);
        if (words > PbFlushMaxWords()) {
            g_pbflush_stats.skipped.fetch_add(1, std::memory_order_relaxed);
            return WrittenPages::None;
        }
        if (!stale) {
            g_pbflush_stats.fresh.fetch_add(1, std::memory_order_relaxed);
        }
    }
    return stale ? WrittenPages::Stale : WrittenPages::Fresh;
}

bool DmaPusher::DeferGpuWrittenEntry(const CommandListHeader& header, std::span<const u32> snapshot) {
    auto& split = VideoCommon::GpuSplit::Get();
    const size_t words = header.size;
    if (!split.Active() || words > DeferredEntry::MaxWords || dma_state.method_count < words ||
        dma_state.method < non_puller_methods) {
        return false;
    }
    auto* const engine = subchannels[dma_state.subchannel];
    if (engine == nullptr) {
        return false;
    }
    const auto type = subchannel_type[dma_state.subchannel];
    if (split.FrontMacros()) {
        // Macros run on the front need the words there.
        if (type == Engines::EngineTypes::Maxwell3D &&
            static_cast<Engines::Maxwell3D*>(engine)->GetNativeMacroEngine().native_turing) {
            return false;
        }
        if (type == Engines::EngineTypes::KeplerCompute &&
            static_cast<Engines::KeplerCompute*>(engine)->FrontRunsMacros()) {
            return false;
        }
    }
    DeferredEntry entry{
        .system = &system,
        .memory = &memory_manager,
        .engine = engine,
        .address = header.addr,
        .words = static_cast<u32>(words),
        .method = dma_state.method,
        .method_count = dma_state.method_count,
        .non_incrementing = dma_state.non_incrementing,
        .increment_once = dma_increment_once,
        .wants_segment = WantsSegment(),
        .payload_dirty = (type == Engines::EngineTypes::KeplerCompute && dma_state.method == ComputeInline &&
                          dma_state.non_incrementing) ||
                         (type == Engines::EngineTypes::Maxwell3D && dma_state.method >= MacroRegistersStart),
        .data{},
    };
    if (snapshot.size() == words) {
        std::ranges::copy(snapshot, entry.data.begin());
    } else {
        memory_manager.ReadBlockUnsafe(header.addr, entry.data.data(), words * sizeof(u32));
    }
    split.Defer([entry] { RunDeferredEntry(entry); });
    g_pbflush_stats.deferred.fetch_add(1, std::memory_order_relaxed);
    // What ProcessCommands does to the method state for these words.
    if (dma_state.non_incrementing) {
        dma_state.is_last_call = true;
    } else {
        for (size_t i = 0; i < words; ++i) {
            dma_state.is_last_call = dma_state.method_count - i <= 1;
            dma_state.method += !dma_state.non_incrementing ? 1 : 0;
            dma_state.non_incrementing |= dma_increment_once;
        }
    }
    dma_state.method_count -= static_cast<u32>(words);
    return true;
}

bool DmaPusher::FlushGpuWrittenCommands(GPUVAddr address, size_t words) {
    // With EDEN_NX2_PBFLUSH_EPOCH, whole pages, so later entries on them need no drain.
    const bool epoch = PbFlushEpochEnabled();
    const GPUVAddr begin = epoch ? address & ~GPUVAddr{0xfff} : address;
    const GPUVAddr end = epoch ? (address + words * sizeof(u32) + 0xfff) & ~GPUVAddr{0xfff}
                               : address + words * sizeof(u32);
    const size_t size = end - begin;
    bool flushed = false;
    const auto flush = [memory = &memory_manager, begin, size, result = &flushed] {
        if (memory->IsMemoryDirty(begin, size, VideoCommon::CacheType::BufferCache)) {
            memory->FlushRegion(begin, size, VideoCommon::CacheType::BufferCache);
            g_pbflush_stats.flushes.fetch_add(1, std::memory_order_relaxed);
            *result = true;
        }
    };
    if (auto& split = VideoCommon::GpuSplit::Get(); split.Active()) {
        // The caches belong to the split's back: flush there, after what the front queued (the
        // work that writes these words among it), and wait for it.
        split.Defer(flush);
        split.Drain();
        g_pbflush_stats.drains.fetch_add(1, std::memory_order_relaxed);
    } else {
        flush();
    }
    if (epoch) {
        for (GPUVAddr page = begin; page < end; page += 0x1000) {
            if (const auto device = memory_manager.GpuToCpuAddress(page)) {
                const u64 device_page = *device >> Nx2WrittenPageBits;
                FreshSlot(device_page) = {device_page, g_writer_epoch};
            }
        }
    }
    if (static std::atomic<u32> reported{}; flushed && reported.fetch_add(1) < 5) {
        LOG_INFO(HW_GPU, "NX2 pbflush: flushed GPU-written command words {:#x}+{:#x} on ch {}",
                 begin, size, channel_state.bind_id);
    }
    return true;
}

void DmaPusher::ReadGpuWrittenWords(GPUVAddr address, std::span<u32> words) const {
    const GPUVAddr end = address + words.size_bytes();
    for (GPUVAddr page = address & ~GPUVAddr{0xfff}; page < end; page += 0x1000) {
        const auto device = memory_manager.GpuToCpuAddress(page);
        if (!device || !Nx2IsGpuWritten(*device)) {
            continue;
        }
        const GPUVAddr begin = (std::max)(page, address);
        const GPUVAddr stop = (std::min)(page + 0x1000, end);
        memory_manager.ReadBlockUnsafe(begin, words.data() + (begin - address) / sizeof(u32), stop - begin);
    }
}

void DmaPusher::CallMethod(u32 argument) {
    if (g_semaphore_watch != 0) [[unlikely]] {
        WatchArgument(dma_state.method, argument);
    }
    auto& split = VideoCommon::GpuSplit::Get();
    if (dma_state.method < non_puller_methods) {
        puller.CallPullerMethod(*this, Engines::Puller::MethodCall{
            dma_state.method,
            argument,
            dma_state.subchannel,
            dma_state.method_count,
        });
    } else {
        auto subchannel = subchannels[dma_state.subchannel];
        if (subchannel == nullptr) {
            ReportUnboundSubchannel();
            return;
        }
        if (subchannel_type[dma_state.subchannel] != Engines::EngineTypes::Maxwell3D) {
            ++g_writer_epoch;
        }
        if (split.Active()) {
            const GPUVAddr segment = dma_state.dma_get + dma_state.dma_word_offset;
            if (split.FrontMacros()) {
                const auto type = subchannel_type[dma_state.subchannel];
                if (type == Engines::EngineTypes::Maxwell3D) {
                    auto* const maxwell3d = static_cast<Engines::Maxwell3D*>(subchannel);
                    if (maxwell3d->GetNativeMacroEngine().native_turing) {
                        maxwell3d->FrontCallMethod(system, split, dma_state.method, argument,
                                                   dma_state.is_last_call);
                        return;
                    }
                } else if (type == Engines::EngineTypes::KeplerCompute) {
                    auto* const compute = static_cast<Engines::KeplerCompute*>(subchannel);
                    if (compute->FrontRunsMacros()) {
                        compute->FrontCallMethod(system, split, dma_state.method, argument,
                                                 dma_state.is_last_call, &segment);
                        return;
                    }
                }
            }
            split.Method(subchannel, dma_state.method, argument, dma_state.is_last_call,
                         WantsSegment() ? &segment : nullptr);
            return;
        }
        if (!subchannel->execution_mask[dma_state.method]) {
            subchannel->method_sink.emplace_back(dma_state.method, argument);
        } else {
            VideoCommon::Nx2Profile::Scope method_scope{VideoCommon::Nx2Profile::Zone::Method};
            subchannel->ConsumeSink(system);
            subchannel->current_dma_segment = dma_state.dma_get + dma_state.dma_word_offset;
            subchannel->CallMethod(system, dma_state.method, argument, dma_state.is_last_call);
        }
    }
}

void DmaPusher::CallMultiMethod(const u32* base_start, u32 num_methods) {
    if (g_semaphore_watch != 0) [[unlikely]] {
        for (u32 i = 0; i < num_methods; ++i) {
            WatchArgument(dma_state.method + (dma_state.non_incrementing ? 0 : i), base_start[i]);
        }
    }
    auto& split = VideoCommon::GpuSplit::Get();
    if (dma_state.method < non_puller_methods) {
        puller.CallMultiMethod(*this, dma_state.method, dma_state.subchannel, base_start, num_methods, dma_state.method_count);
    } else {
        auto subchannel = subchannels[dma_state.subchannel];
        if (subchannel == nullptr) {
            ReportUnboundSubchannel();
            return;
        }
        if (subchannel_type[dma_state.subchannel] != Engines::EngineTypes::Maxwell3D) {
            ++g_writer_epoch;
        }
        if (split.Active()) {
            const GPUVAddr segment = dma_state.dma_get + dma_state.dma_word_offset;
            if (split.FrontMacros()) {
                const auto type = subchannel_type[dma_state.subchannel];
                if (type == Engines::EngineTypes::Maxwell3D) {
                    auto* const maxwell3d = static_cast<Engines::Maxwell3D*>(subchannel);
                    if (maxwell3d->GetNativeMacroEngine().native_turing) {
                        maxwell3d->FrontCallMultiMethod(system, split, dma_state.method, base_start,
                                                        num_methods, dma_state.method_count);
                        return;
                    }
                } else if (type == Engines::EngineTypes::KeplerCompute) {
                    auto* const compute = static_cast<Engines::KeplerCompute*>(subchannel);
                    if (compute->FrontRunsMacros()) {
                        compute->FrontCallMultiMethod(system, split, dma_state.method, base_start,
                                                      num_methods, dma_state.method_count, &segment);
                        return;
                    }
                }
            }
            split.MultiMethod(subchannel, dma_state.method, base_start, num_methods,
                              dma_state.method_count, WantsSegment() ? &segment : nullptr);
            return;
        }
        VideoCommon::Nx2Profile::Scope method_scope{VideoCommon::Nx2Profile::Zone::Method};
        subchannel->ConsumeSink(system);
        subchannel->current_dma_segment = dma_state.dma_get + dma_state.dma_word_offset;
        subchannel->CallMultiMethod(system, dma_state.method, base_start, num_methods, dma_state.method_count);
    }
}

void DmaPusher::ReportUnboundSubchannel() {
    // Methods sent to a subchannel with no engine (no SetObject yet) are dropped.
    static std::atomic<u32> reported{};
    if (reported.fetch_add(1) < 8) {
        LOG_WARNING(HW_GPU, "Dropping method {:#x} on unbound subchannel {}", dma_state.method,
                    dma_state.subchannel);
    }
}

void DmaPusher::CheckSubmitHash(const CommandListHeader& header, u64 submit_hash) {
    std::vector<u32> words(header.size);
    memory_manager.ReadBlockUnsafe(header.addr, words.data(), words.size() * sizeof(u32));
    static std::atomic<u64> checked{};
    static std::atomic<u64> changed{};
    ++checked;
    if (HashPushbufferWords(words) != submit_hash) {
        if (changed.fetch_add(1) < 16) {
            LOG_WARNING(HW_GPU, "pushbuffer entry {:#x}+{:#x} changed between submit and execution",
                        header.addr.Value(), header.size.Value());
        }
    }
    if ((checked % 20000) == 0) {
        LOG_WARNING(HW_GPU, "pushbuffer check: {} of {} entries changed after submit",
                    changed.load(), checked.load());
    }
}

static std::atomic<const char*> g_entry_dump_reason{};

void RequestEntryDump(const char* reason) {
    g_entry_dump_reason.store(reason);
}

void DmaPusher::HandleEntryDumpRequest() {
    if (const char* reason = g_entry_dump_reason.exchange(nullptr)) {
        DumpCurrentEntry(reason);
    }
}

void DmaPusher::CheckSubmitWords(const CommandListHeader& header,
                                 std::span<const u32> submit_words) {
    std::vector<u32> words(submit_words.size());
    memory_manager.ReadBlockUnsafe(header.addr, words.data(), words.size() * sizeof(u32));
    static std::atomic<u64> checked{};
    static std::atomic<u64> changed{};
    ++checked;
    size_t differing = 0;
    size_t first = 0;
    std::array<std::string, 4> diffs;
    for (size_t i = 0; i < words.size(); ++i) {
        if (words[i] == submit_words[i]) {
            continue;
        }
        if (differing == 0) {
            first = i;
        }
        if (differing < diffs.size()) {
            diffs[differing] = fmt::format("[{}] {:08x}->{:08x}", i, submit_words[i], words[i]);
        }
        ++differing;
    }
    if (differing != 0 && changed.fetch_add(1) < 32) {
        LOG_WARNING(HW_GPU,
                    "pushbuffer copy: ch {} entry {:#x}+{:#x} has {} words changed since submit "
                    "(first {}): {} (previous entry suspended: {})",
                    channel_state.bind_id, header.addr.Value(), header.size.Value(), differing,
                    first, fmt::join(diffs.begin(), diffs.begin() + std::min<size_t>(differing, diffs.size()), " "),
                    previous_entry_suspended);
    }
    if ((checked % 20000) == 0) {
        LOG_WARNING(HW_GPU, "pushbuffer copy: {} of {} entries differ from submit",
                    changed.load(), checked.load());
    }
}

void DmaPusher::RecordParsedEntry(const CommandListHeader& header,
                                  std::span<const CommandHeader> commands) {
    if (!g_record_parsed_entries) {
        return;
    }
    u64 hash = 0xcbf29ce484222325ULL;
    for (const CommandHeader& command : commands) {
        hash = (hash ^ command.argument) * 0x100000001b3ULL;
    }
    parsed_entries[parsed_entry_count++ % parsed_entries.size()] = {header.addr, static_cast<u32>(header.size), hash};
}

void DmaPusher::DumpCurrentEntry(const char* reason) {
    // Entries whose memory changed after they were parsed (the CPU rewrote them meanwhile).
    for (size_t i = 0; i < std::min(parsed_entry_count, parsed_entries.size()); ++i) {
        const auto& entry = parsed_entries[(parsed_entry_count - 1 - i) % parsed_entries.size()];
        std::vector<u32> words(entry.size);
        memory_manager.ReadBlockUnsafe(entry.address, words.data(), words.size() * sizeof(u32));
        u64 hash = 0xcbf29ce484222325ULL;
        for (const u32 word : words) {
            hash = (hash ^ word) * 0x100000001b3ULL;
        }
        LOG_WARNING(HW_GPU, "  parsed -{}: {:#x}+{:#x} {}", i, entry.address, entry.size,
                    hash == entry.hash ? "unchanged" : "CHANGED since parsed");
    }
    // EDEN_NX2_PB_DUMP=<file>: also write the whole command list (u64 address, u32 size and the
    // current words of every entry, then the current entry index) for offline replay.
    if (static const char* const path = std::getenv("EDEN_NX2_PB_DUMP");
        path != nullptr && !dma_pushbuffer.empty()) {
        if (std::FILE* file = std::fopen(path, "wb")) {
            const auto& lists = dma_pushbuffer.front().command_lists;
            const u64 current = dma_pushbuffer_subindex;
            const u64 count = lists.size();
            std::fwrite(&count, sizeof(count), 1, file);
            std::fwrite(&current, sizeof(current), 1, file);
            for (const CommandListHeader& header : lists) {
                const u64 address = header.addr;
                const u32 size = static_cast<u32>(header.size);
                std::vector<u32> words(size);
                memory_manager.ReadBlockUnsafe(address, words.data(), words.size() * sizeof(u32));
                std::fwrite(&address, sizeof(address), 1, file);
                std::fwrite(&size, sizeof(size), 1, file);
                std::fwrite(words.data(), sizeof(u32), words.size(), file);
            }
            std::fclose(file);
        }
    }
    LOG_WARNING(HW_GPU, "pushbuffer dump ({}): method={:#x} sub={} count={} non_inc={} get={:#x}",
                reason, dma_state.method, dma_state.subchannel, dma_state.method_count,
                dma_state.non_incrementing, dma_state.dma_get);
    if (dma_pushbuffer.empty()) {
        return;
    }
    const auto& lists = dma_pushbuffer.front().command_lists;
    const size_t first = dma_pushbuffer_subindex >= 2 ? dma_pushbuffer_subindex - 2 : 0;
    for (size_t i = first; i <= dma_pushbuffer_subindex && i < lists.size(); ++i) {
        const CommandListHeader& header = lists[i];
        LOG_WARNING(HW_GPU, "  entry {} of {}: addr={:#x} size={:#x}", i, lists.size(),
                    header.addr.Value(), header.size.Value());
        for (u64 word = 0; word < std::min<u64>(header.size, 0x80); word += 8) {
            std::string gpu_line;
            std::string cpu_line;
            for (u64 j = word; j < std::min<u64>(header.size, word + 8); ++j) {
                const GPUVAddr address = header.addr + j * sizeof(u32);
                const u32 gpu_value = memory_manager.Read<u32>(address);
                const u32 cpu_value = system.ApplicationMemory().Read32(address);
                gpu_line += fmt::format(" {:08x}", gpu_value);
                cpu_line += gpu_value == cpu_value ? std::string(" ........")
                                                   : fmt::format(" {:08x}", cpu_value);
            }
            LOG_WARNING(HW_GPU, "    +{:03x}:{}", word * 4, gpu_line);
            LOG_WARNING(HW_GPU, "    cpu :{}", cpu_line);
        }
    }
}

void DmaPusher::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
}

} // namespace Tegra
