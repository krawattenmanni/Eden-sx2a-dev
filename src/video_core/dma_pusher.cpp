// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <optional>
#include <vector>

#include "common/logging.h"
#include "common/settings.h"
#include "core/core.h"
#include "video_core/dma_pusher.h"
#include "video_core/control/channel_state.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/gpu.h"
#include "video_core/gpu_split.h"
#include "video_core/guest_memory.h"
#include "video_core/memory_manager.h"
#include "video_core/nx2.h"
#include "video_core/nx2_profile.h"
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
    std::atomic<u64> fresh{};               ///< Hits whose pages were flushed since the last writer
    std::atomic<u64> drains{};
    std::atomic<u64> flushes{};
    /// Stale hits on a page flushed in an older epoch, by the engine whose work ended it
    std::array<std::atomic<u64>, 6> stale_epoch{};
    std::atomic<u64> stale_other{}; ///< Stale hits on a page not in the fresh table
};
PbFlushStats g_pbflush_stats;
// The engine of the last method that moved the writer epoch (EDEN_NX2_PBFLUSH_STATS).
Engines::EngineTypes g_last_writer{};
// Read on first use: Android sets the switches after the library is loaded.
bool PbFlushStatsEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("EDEN_NX2_PBFLUSH_STATS");
        return value != nullptr && value[0] != '0';
    }();
    return enabled;
}
// EDEN_NX2_PBFLUSH_MAX: largest entry flushed, in words (diagnostics; no limit by default).
size_t PbFlushMaxWords() {
    static const size_t words = [] {
        const char* value = std::getenv("EDEN_NX2_PBFLUSH_MAX");
        return value != nullptr ? static_cast<size_t>(std::strtoull(value, nullptr, 0))
                                : (std::numeric_limits<size_t>::max)();
    }();
    return words;
}

// EDEN_NX2_PBFLUSH_EPOCH (on by default for Switch 2 programs, =0 off): an entry on pages flushed
// (with a drain) since the front last sent work to an engine other than 3D is read without another
// drain. Assumes the words are written by compute or copies, not by 3D draws.
bool PbFlushEpochEnabled() {
    static const bool enabled = Nx2EnvDefaultOn("EDEN_NX2_PBFLUSH_EPOCH");
    return enabled && Nx2Program();
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
    LOG_INFO(HW_GPU, "NX2 pbflush stats: checked {} hits <=8 {} <=64 {} >64 {} skipped {} fresh {} drains {} flushes {} | stale after compute {} dma {} i2m {} 2d {} other {} | not in table {}",
             take(s.checked), take(s.hits[0]), take(s.hits[1]), take(s.hits[2]), take(s.skipped),
             take(s.fresh), take(s.drains), take(s.flushes),
             take(s.stale_epoch[static_cast<size_t>(Engines::EngineTypes::KeplerCompute)]),
             take(s.stale_epoch[static_cast<size_t>(Engines::EngineTypes::MaxwellDMA)]),
             take(s.stale_epoch[static_cast<size_t>(Engines::EngineTypes::KeplerMemory)]),
             take(s.stale_epoch[static_cast<size_t>(Engines::EngineTypes::Fermi2D)]),
             take(s.stale_epoch[static_cast<size_t>(Engines::EngineTypes::Nv01Timer)]) +
                 take(s.stale_epoch[static_cast<size_t>(Engines::EngineTypes::Maxwell3D)]),
             take(s.stale_other));
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
    dma_state.is_last_call = true;
    // Guest CPU writes held back for GPU-written pages are applied before this submit.
    Nx2DmaReadWriter();
    auto& split = VideoCommon::GpuSplit::Get();
    if (split.Active()) {
        split.SetAfterMethod(channel_state.native_abi ? &FailedNativeWorkHook : nullptr, this);
    }
    while (system.IsPoweredOn()) {
        if (!Step()) {
            break;
        }
    }
    if (split.Active()) {
        split.Defer([gpu = &system.GPU()] {
            gpu->FlushCommands();
            gpu->OnCommandListEnd();
        });
        split.Publish();
        return;
    }
    VideoCommon::Nx2Profile::Scope list_end_scope{VideoCommon::Nx2Profile::Zone::ListEnd};
    system.GPU().FlushCommands();
    system.GPU().OnCommandListEnd();
}

bool DmaPusher::HasFailedNativeWork() const {
    // The failure flags belong to the back thread when the GPU thread is split.
    if (VideoCommon::GpuSplit::Get().Active()) {
        return false;
    }
    return CheckFailedNativeWork();
}

void DmaPusher::FailedNativeWorkHook(void* dma_pusher) {
    static_cast<const DmaPusher*>(dma_pusher)->CheckFailedNativeWork();
}

bool DmaPusher::CheckFailedNativeWork() const {
    const auto& context = channel_state.native_macro_context;
    if (!channel_state.native_abi || !context ||
        !(context->engine.native_execution_failed || context->compute_engine.native_execution_failed)) {
        return false;
    }
    // EDEN_NATIVE_STRICT keeps the channel stalled for debugging; otherwise drop the failed work
    // (a skipped draw or dispatch) and keep the game running instead of hanging on its fences.
    static const bool strict = std::getenv("EDEN_NATIVE_STRICT") != nullptr;
    if (strict) {
        return true;
    }
    static std::atomic<u32> reported{};
    if (reported.fetch_add(1) < 32) {
        LOG_WARNING(HW_GPU, "Native GPU work failed; skipping it and continuing the channel");
    }
    context->engine.native_execution_failed = false;
    context->compute_engine.native_execution_failed = false;
    return false;
}

bool DmaPusher::Step() {
    if (HasFailedNativeWork()) return false;
    if (!ib_enable || dma_pushbuffer.empty()) {
        return false;
    }

    if (!puller.ResumeNativeAcquire(*this)) return false;

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
        command_resume_offset += ProcessCommands(commands.subspan(command_resume_offset), command_resume_offset);
        if (puller.HasPendingNativeAcquire() || HasFailedNativeWork()) return false;
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
        bool gpu_words = false;
        std::optional<VideoCommon::Nx2Profile::Scope> entry_scope;
        entry_scope.emplace(VideoCommon::Nx2Profile::Zone::EntryCheck);
        if (subchannels[dma_state.subchannel] && dma_state.method_count) {
            const auto engine = subchannel_type[dma_state.subchannel];
            const bool kepler_payload = engine == Engines::EngineTypes::KeplerCompute && dma_state.method == ComputeInline && dma_state.non_incrementing;
            const bool macro_payload = engine == Engines::EngineTypes::Maxwell3D && dma_state.method >= MacroRegistersStart;
            if (kepler_payload || macro_payload) {
                const size_t words = std::min<size_t>(dma_state.method_count, header.size);
                auto* const engine_state = subchannels[dma_state.subchannel];
                // Parameters continued in an entry of their own can be arguments the GPU wrote
                // (an indirect draw record a culling pass filled in). Unsafe reads would give the
                // macro the older guest words, so flush those first. Safe reads (below) already
                // flush. EDEN_NX2_PBFLUSH=0 turns this off.
                static const bool flush_parameters = [] {
                    const char* value = std::getenv("EDEN_NX2_PBFLUSH");
                    return value == nullptr || value[0] != '0';
                }();
                const bool flushed_parameters =
                    flush_parameters && words <= 64 &&
                    FlushGpuWrittenCommands(dma_state.dma_get, words * sizeof(u32), &gpu_words);
                if (flushed_parameters) {
                    engine_state->current_dirty = false;
                } else if (auto& split = VideoCommon::GpuSplit::Get(); split.Active()) {
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
        // A safe read flushes the caches, which belong to the split's back thread while it runs
        // (the split only starts with unsafe reads; the settings can change mid-game).
        const bool use_safe = !VideoCommon::GpuSplit::Get().Active() &&
                              (Settings::IsDMALevelDefault() ? Settings::IsGPULevelHigh() : Settings::IsDMALevelSafe());
        if (const u32* const snapshot = command_list.Snapshot(dma_pushbuffer_subindex)) {
            std::span<const u32> words{snapshot, header.size};
            CheckSnapshot(dma_state.dma_get, words);
            FlushGpuWrittenCommands(dma_state.dma_get, words.size_bytes(), &gpu_words);
            if (gpu_words) {
                // The snapshot predates the GPU work that writes these words: take the pages the
                // GPU wrote from guest memory, now flushed, and the rest from the snapshot.
                gpu_written_words.assign(words.begin(), words.end());
                ReadGpuWrittenWords(dma_state.dma_get, gpu_written_words);
                words = gpu_written_words;
            }
            entry_scope.reset();
            const std::span<const CommandHeader> commands{
                reinterpret_cast<const CommandHeader*>(words.data()), words.size()};
            command_resume_offset += ProcessCommands(commands.subspan(command_resume_offset), command_resume_offset);
        } else if (use_safe) {
            entry_scope.reset();
            Tegra::Memory::GpuGuestMemory<Tegra::CommandHeader, Tegra::Memory::GuestMemoryFlags::SafeRead>headers(memory_manager, dma_state.dma_get, header.size, &command_headers);
            const std::span<const CommandHeader> commands{headers.data(), headers.size()};
            command_resume_offset += ProcessCommands(commands.subspan(command_resume_offset), command_resume_offset);
        } else {
            FlushGpuWrittenCommands(dma_state.dma_get, size_t{header.size} * sizeof(u32));
            entry_scope.reset();
            Tegra::Memory::GpuGuestMemory<Tegra::CommandHeader, Tegra::Memory::GuestMemoryFlags::UnsafeRead>headers(memory_manager, dma_state.dma_get, header.size, &command_headers);
            const std::span<const CommandHeader> commands{headers.data(), headers.size()};
            command_resume_offset += ProcessCommands(commands.subspan(command_resume_offset), command_resume_offset);
        }
    }

    if (puller.HasPendingNativeAcquire() || HasFailedNativeWork()) return false;
    command_resume_offset = 0;
    if (++dma_pushbuffer_subindex >= command_list_size) {
        dma_pushbuffer.pop();
        dma_pushbuffer_subindex = 0;
    } else {
        signal_sync = command_list.command_lists[dma_pushbuffer_subindex].sync && Settings::values.sync_memory_operations.GetValue();
    }

    if (signal_sync) {
        VideoCommon::GpuSplit::Get().Drain(VideoCommon::Nx2Profile::DrainSite::SyncMemory);
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
            if (channel_state.native_abi && dma_state.method < non_puller_methods) max_write = 1;
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
        if (puller.HasPendingNativeAcquire() || HasFailedNativeWork()) return index;
    }
    return commands.size();
}

void DmaPusher::SetState(const CommandHeader& command_header) {
    dma_state.method = command_header.method;
    dma_state.subchannel = command_header.subchannel;
    dma_state.method_count = command_header.method_count;
}

void DmaPusher::CallMethod(u32 argument) {
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
            ++g_nx2_writer_epoch;
            g_last_writer = subchannel_type[dma_state.subchannel];
        }
        if (auto& split = VideoCommon::GpuSplit::Get(); split.Active()) {
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
            subchannel->ConsumeSink(system);
            subchannel->current_dma_segment = dma_state.dma_get + dma_state.dma_word_offset;
            subchannel->CallMethod(system, dma_state.method, argument, dma_state.is_last_call);
        }
    }
}

void DmaPusher::CallMultiMethod(const u32* base_start, u32 num_methods) {
    if (dma_state.method < non_puller_methods) {
        puller.CallMultiMethod(*this, dma_state.method, dma_state.subchannel, base_start, num_methods, dma_state.method_count);
    } else {
        auto subchannel = subchannels[dma_state.subchannel];
        if (subchannel == nullptr) {
            ReportUnboundSubchannel();
            return;
        }
        if (subchannel_type[dma_state.subchannel] != Engines::EngineTypes::Maxwell3D) {
            ++g_nx2_writer_epoch;
            g_last_writer = subchannel_type[dma_state.subchannel];
        }
        if (auto& split = VideoCommon::GpuSplit::Get(); split.Active()) {
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
        subchannel->ConsumeSink(system);
        subchannel->current_dma_segment = dma_state.dma_get + dma_state.dma_word_offset;
        subchannel->CallMultiMethod(system, dma_state.method, base_start, num_methods, dma_state.method_count);
    }
}

void DmaPusher::ReportUnboundSubchannel() const {
    // Methods sent to a subchannel with no engine (no SetObject yet) are dropped.
    static std::atomic<u32> reported{};
    if (reported.fetch_add(1) < 8) {
        LOG_WARNING(HW_GPU, "Dropping method {:#x} on unbound subchannel {}", dma_state.method,
                    dma_state.subchannel);
    }
}

bool DmaPusher::WantsSegment() const {
    // Only macro parameters and compute uploads read the engine's current_dma_segment.
    const auto engine = subchannel_type[dma_state.subchannel];
    return engine == Engines::EngineTypes::KeplerCompute ||
           (engine == Engines::EngineTypes::Maxwell3D && dma_state.method >= MacroRegistersStart);
}

bool DmaPusher::FlushGpuWrittenCommands(GPUVAddr address, size_t size, bool* gpu_words) {
    // Mario Kart World runs small command lists its culling dispatches fill in each frame. Safe
    // reads flush every command list; unsafe ones only flush those on pages the GPU has written,
    // or their draws run with an older frame's words. Only those pages wait for the split's back.
    if (!Nx2Program() || size == 0) {
        return false;
    }
    const bool stats = PbFlushStatsEnabled();
    if (stats) {
        ReportPbFlushStats();
        g_pbflush_stats.checked.fetch_add(1, std::memory_order_relaxed);
    }
    const bool epoch = PbFlushEpochEnabled();
    bool written = false;
    bool stale = false;
    const GPUVAddr end = address + size;
    for (GPUVAddr page = address & ~GPUVAddr{0xfff}; page < end; page += 0x1000) {
        const auto device = memory_manager.GpuToCpuAddress(page);
        if (!device || !Nx2IsGpuWritten(*device)) {
            continue;
        }
        written = true;
        const u64 device_page = *device >> Nx2WrittenPageBits;
        if (!epoch || !Nx2IsFresh(device_page)) {
            stale = true;
            if (stats && epoch) {
                if (Nx2FreshSlot(device_page).device_page == device_page) {
                    g_pbflush_stats.stale_epoch[static_cast<size_t>(g_last_writer)].fetch_add(
                        1, std::memory_order_relaxed);
                } else {
                    g_pbflush_stats.stale_other.fetch_add(1, std::memory_order_relaxed);
                }
            }
            break;
        }
    }
    if (!written) {
        return false;
    }
    const size_t words = size / sizeof(u32);
    if (stats) {
        g_pbflush_stats.hits[words <= 8 ? 0 : words <= 64 ? 1 : 2].fetch_add(1, std::memory_order_relaxed);
    }
    if (words > PbFlushMaxWords()) {
        if (stats) {
            g_pbflush_stats.skipped.fetch_add(1, std::memory_order_relaxed);
        }
        return false;
    }
    if (!stale) {
        // Flushed since the front last sent work that may write these pages.
        if (gpu_words) {
            *gpu_words = true;
        }
        if (stats) {
            g_pbflush_stats.fresh.fetch_add(1, std::memory_order_relaxed);
        }
        return false;
    }
    // With EDEN_NX2_PBFLUSH_EPOCH, whole pages, so later entries on them need no drain.
    const GPUVAddr flush_begin = epoch ? address & ~GPUVAddr{0xfff} : address;
    const GPUVAddr flush_end = epoch ? (end + 0xfff) & ~GPUVAddr{0xfff} : end;
    const size_t flush_size = flush_end - flush_begin;
    // The caches belong to the split's back: it first runs what the front queued (the dispatch
    // that writes these words among it).
    VideoCommon::GpuSplit::Get().Drain(VideoCommon::Nx2Profile::DrainSite::PbFlush);
    if (stats) {
        g_pbflush_stats.drains.fetch_add(1, std::memory_order_relaxed);
    }
    if (epoch) {
        for (GPUVAddr page = flush_begin; page < flush_end; page += 0x1000) {
            if (const auto device = memory_manager.GpuToCpuAddress(page)) {
                Nx2MarkFresh(*device >> Nx2WrittenPageBits);
            }
        }
    }
    VideoCommon::Nx2Profile::Scope flush_scope{VideoCommon::Nx2Profile::Zone::PbFlushFlush};
    if (!memory_manager.IsMemoryDirty(flush_begin, flush_size, VideoCommon::CacheType::BufferCache)) {
        return false;
    }
    memory_manager.FlushRegion(flush_begin, flush_size, VideoCommon::CacheType::BufferCache);
    if (stats) {
        g_pbflush_stats.flushes.fetch_add(1, std::memory_order_relaxed);
    }
    if (gpu_words) {
        *gpu_words = true;
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

void DmaPusher::CheckSnapshot(GPUVAddr address, std::span<const u32> snapshot) {
    static const bool check = [] {
        const char* value = std::getenv("EDEN_NX2_PB_CHECK");
        return value != nullptr && value[0] != '0';
    }();
    if (!check) {
        return;
    }
    static std::atomic<u64> checked{};
    static std::atomic<u64> changed{};
    std::vector<u32> words(snapshot.size());
    memory_manager.ReadBlockUnsafe(address, words.data(), words.size() * sizeof(u32));
    size_t differing = 0;
    size_t first = 0;
    for (size_t i = 0; i < words.size(); ++i) {
        if (words[i] != snapshot[i] && differing++ == 0) {
            first = i;
        }
    }
    const u64 total = checked.fetch_add(1) + 1;
    if (differing != 0 && changed.fetch_add(1) < 32) {
        LOG_WARNING(HW_GPU, "NX2 pb copy: ch {} entry {:#x}+{:#x}: {} words changed since submit (first [{}] {:08x}->{:08x})",
                    channel_state.bind_id, address, snapshot.size(), differing, first, snapshot[first], words[first]);
    }
    if (total % 20000 == 0) {
        LOG_INFO(HW_GPU, "NX2 pb copy: {} of {} entries changed since submit", changed.load(), total);
    }
}

void DmaPusher::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
}

} // namespace Tegra
