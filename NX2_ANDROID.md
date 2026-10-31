# Eden NX2 on Android (branch `nx2-android`)

Port of Takoroka's `eden-nx2` (experimental Switch 2 support, Windows-only) to Eden's Android app, plus
fixes found while booting Donkey Kong Bananza on a phone.
See `NX2_NOTES.md` for the original author's notes (blockers, env switches, title status).

## Current state (2026-10-30, 18:35, parked)

DK Bananza plays on Turnip at ~12-16 FPS with the GPU split and macro JIT back on (run 37, no
crash in ~5 min). Fixed today: garbage-command crashes (`EDEN_NX2_PB_COPY`, submit-time pushbuffer
snapshots), purple screen after heavy scenes (DLSS stand-in finds its images by scanning handles),
missing ground, and the destructible terrain's culling pass (native geometry shaders + Generic31
fix). All 531-650 dumped shaders lower (`tools/nx2/lowertest`).

Open, main blocker: destructible terrain (walls, much of the ground) draws solid black, and the main
menu has no wallpaper (user's pass/fail check; likely the same cause). Eden-NS2 shows black on
NVIDIA too. Ruled out: shader rejections, skipped draws, the passthrough skip mask (all generics
passed), GS global-store tracking (both buffers become storage buffers), missing storage barriers
(run 38). The culling pass writes per-chunk triangle counters and u16 index buffers, but in a
6000-event trace no draw read them (run 39). Next: find what consumes c0[0x410]/c0[0x420] (maybe a
later frame, a compute pass on another channel, or memory the buffer cache never syncs from the
host GPU back to guest memory so the guest/CPU sees zeros); trace with `EDEN_NX2_DRAW_TRACE_FRAME`
to cover a whole frame; check the terrain's colour pass inputs (3D voxel textures inserted in
run 37?).

Also open: port the useful parts of Eden-NS2 `e2f285ab1` (scanned clean: buffer-cache mapped
prefix cache, ASTC no-wait/2048 cap, 3 capture sets); abort on app close with the split on
(`PresentManager::CopyToSwapchain`); make `EDEN_NX2_PB_COPY` the default for Switch 2; Joy-Con
colours. Code changes are done by subagents; the main session tests and documents.

Run with: `EDEN_NX2_NO_OCCLUSION=1 EDEN_NX2_PB_COPY=1` (split and macro JIT on by default).
Diagnostics: `EDEN_NX2_PB_CHECK`, `EDEN_NX2_PB_DUMP=<file>`, `EDEN_NX2_SHADER_DUMP=<abs dir>`,
`EDEN_NX2_DRAW_TRACE=<N>` (+ `_GS=<hex>`, `_FRAME=<n>`), `EDEN_NX2_SSBO_BARRIER=1`.
Wireless adb works (`adb mdns services`, then `adb connect <ip:port>`); disconnect it when USB is
plugged in, or the scripts see two devices.

## Where things are

| What | Path |
|---|---|
| Repo (git, branch `nx2-android`) | `<repo>` |
| Original download (no git, untouched) | `<takoroka-eden-nx2>` |
| APK output | `artifacts/app-mainline-relWithDebInfo.apk` |
| Unstripped lib (for symbolizing) | `src/android/app/build/intermediates/cxx/RelWithDebInfo/*/obj/arm64-v8a/libyuzu-android.so` |
| Build log | `build-android.log` (git-ignored via `.git/info/exclude`, as is `artifacts/`) |
| Game dump (Mac) | `<dump-dir>/DonkeyKongBananza/{base,updates/458752,dlc}` |
| Game on phone | `/sdcard/Switch2/DonkeyKongBananza/458752/{exefs,romfs}` |
| DLC on phone | `/sdcard/Switch2/DonkeyKongBananza/dlc/040074a01bf13001/0/romfs` |
| App files on phone | `/sdcard/Android/data/dev.eden.nx2.relWithDebInfo/files/` (`log/`, `config/config.ini`, `nx2_env.txt`) |
| Macro JIT fuzz harness | `tools/nx2/jit_fuzz/` |

Remotes and refs: `origin` is upstream Eden (`https://git.eden-emu.dev/eden-emu/eden.git`), cloned
with `--filter=blob:none`. `takoroka/eden-nx2` and `takoroka/nx2-2026-10-29` are fetched from
Takoroka's repo, `ns2/main` from the Eden-NS2 fork (see Related forks). Do not push.

## Commits on top of upstream `26a03d247`

| Commit | Change |
|---|---|
| `faf835afb` | Takoroka's nx2-2026-10-29 source, applied as one diff (scanned for malicious code first: none found). Its tree is identical to Takoroka's tag `nx2-2026-10-29` |
| `cd81803f4` | NCE: visitor stubs for the new A64 decoder entries (Android build fix) |
| `15590f236` | Android: "Launch ExeFS game folder" on the add-folder button (needs All files access; boots `exefs/main` by real path) |
| `02dbc6a54` | Loose DLC: `dlc/<title id>/<version>/romfs` next to/above the game folder is listed by `aoc:u` and served by `OpenDataStorageByDataId` |
| `65e3a0e81` | arm64 (oaknut) backend for the Turing macro JIT; fuzzed against x86-64 JIT and interpreter (100k programs, 0 mismatches), ~5x faster than the interpreter |
| `d53ee0e3d` | Android: 10GB / 12GB memory layout choices |
| `e6d15e9c4` | Kernel: Switch 2 heaps may exceed 8 GiB in svcSetHeapSize's size check |
| `babcf2a08` | This document and the fuzz harness |
| `50a68a0cb` | Kernel: Switch 2 processes get a 16 GiB heap region (`KProcess::Switch2HeapRegionSize`) |
| `cf001fb06` | Android: `EDEN_NX2_*` switches from `files/nx2_env.txt` |
| `a767d265c` | video_core: up to 32 storage buffers per stage; Qualcomm/ARM cap uses the plain (not dynamic) storage buffer limit |
| `a66ba9fa1` | nx2: crop presents to vi2 229 word 7 (visible size); quiet vi2 107 |
| `0d5337c52` | vk_query_cache: `EDEN_NX2_NO_OCCLUSION` diagnostics switch |
| `a4e388c17` | Android: `exefs_path` intent extra, so adb can launch a loose ExeFS game directly |
| `411b16f8b` | host_memory: zero fills punch holes in the memfd instead of memset (heap clearing no longer commits the whole heap) |
| `4721f5a89` | This document's run log and `tools/nx2/android` phone scripts |
| `4a36de89e` | Fibers: stacks left uninitialized (zeroing committed 4 MiB per guest thread, ~490 MiB) |
| `e44c49197` | Dynarmic arm64: per-block relocation/fastmem info in vectors, not hash maps (block metadata was >1 GiB) |
| `4d38a0cc7` | Diagnostics: `EDEN_NX2_ALLOC_PROFILE` (sampling operator new profiler), `EDEN_NX2_TICK_TRACE` |
| `3bc9b850b` | Native shader lowering: LDC U8/S8/U16/S16, VOTE.U into UR16-27, F2I round-up |
| `a31a3088a` | Native shader lowering: MUFU with a uniform source (`0xd08`), register HSETP2 (`0x234`) |
| `28980992a` | Skip draws with topology past Patches and syncpoint increments past 192 |
| `c5a31d62d`..`b1f075aa1` | QMD re-read and remap, flush on channel wait, F2I S8, out-of-range syncpoints, GPU maps in GPU order (runs 24-28) |
| `34cc3abff` | `EDEN_NX2_PB_COPY`: execute GPFIFO entries from submit-time snapshots; dump on first macro desync |
| `043b05709` | Upscaler stand-in finds its input/output by scanning handles (DKB's DLSS pass, purple screen) |
| `4645105e4` | Generic31Y/Z/W count as generic attributes (upstream bug) |
| `39e6f2ba4` | Native geometry shaders (ported from Eden-NS2 `de4f49702`), IMAD.X, LEA.HI, F2I U16 trunc |
| `677723424` | `tools/nx2/lowertest` offline lowering check |
| this commit's parent | `EDEN_NX2_DRAW_TRACE`, `EDEN_NX2_SSBO_BARRIER` diagnostics |

## Build (macOS host)

Toolchain installed for this: Android SDK at `~/Library/Android/sdk` with cmdline-tools, `cmake;3.31.5`,
`ndk;28.2.13676358`; Temurin JDK 17; `brew install glslang`.

```sh
cd <repo>
export ANDROID_HOME=$HOME/Library/Android/sdk JAVA_HOME=/Library/Java/JavaVirtualMachines/temurin-17.jdk/Contents/Home
sh .ci/android/build.sh -t standard -b RelWithDebInfo > build-android.log 2>&1
~/Library/Android/sdk/platform-tools/adb install --user 0 -r artifacts/app-mainline-relWithDebInfo.apk
```

Always install with `--user 0`, so the app never lands in a work profile or app-cloner user. Incremental builds take ~15-60 s.

RelWithDebInfo installs as `dev.eden.nx2.relWithDebInfo`, beside a normal Eden install.
For a quick compile check of one file, reuse the command from
`src/android/app/.cxx/RelWithDebInfo/*/arm64-v8a/compile_commands.json`.

## Phone and settings

Honor PNM-N49 (foldable: inner 2172x2352, outer 2420x1080), Snapdragon SM8850, Adreno 840,
16 GB RAM (15 GB visible), Android 17 (user build), Qualcomm driver 512.842.44.

- Memory layout: 12GB (Switch 2, Unsafe). Switch 2 programs also switch to it automatically.
- CPU backend: Dynarmic (layouts above 4GB need it; NCE won't work)
- CPU accuracy: Auto
- GPU Mode: Fast (the notes' `gpu_accuracy=Low`)
- Async presentation: off (crash race, NX2_NOTES row 118). The Android UI has no toggle for it (it is
  not "GPU async emulation", which stays on): set `async_presentation=false` and
  `async_presentation\default=false` in `files/config/config.ini` with the app stopped
- No keys or firmware needed for loose ExeFS titles; skip them in the setup wizard
- Unknown service commands of Switch 2 programs are auto-stubbed without the Auto Stub setting

Launch: folder button, "Launch ExeFS game folder", pick `Switch2/DonkeyKongBananza/458752`.
Loading takes a while on a black screen; that alone is not a hang.

### Env switches

`EDEN_NX2_*` switches (listed in NX2_NOTES) are read from `files/nx2_env.txt` at app start, one
`KEY=VALUE` per line, `#` comments allowed. Delete the file to turn them off.

```sh
printf 'EDEN_NX2_MEM_TRACE=1\n' > /tmp/nx2_env.txt
adb push /tmp/nx2_env.txt /sdcard/Android/data/dev.eden.nx2.relWithDebInfo/files/
```

Useful here: `EDEN_NX2_MEM_TRACE` (heap and GetInfo memory queries), `EDEN_NX2_SVC_TRACE` (every SVC;
floods the log), `EDEN_NX2_NO_OCCLUSION` (this branch: no host occlusion queries).

## Donkey Kong Bananza (`040074a01bf12000`, update v458752)

The update folder's romfs is the full patched romfs (same size as base), so it boots on its own.

| Run | Result | Cause | Fix |
|---|---|---|---|
| 1 | abort `0xCA01` (InvalidSize) at boot: `nninitStartup -> nn::init::Start -> nn::os::SetHeapSize -> nn::diag::Abort` | svcSetHeapSize rejected sizes >= 8 GiB | `e6d15e9c4` |
| 2 | black screen, 100 MB log of `ExceptionRaised(exception = 0, ... code = 00000000)` | SetHeapSize(0x261600000, ~9.5 GiB = GetInfo 21 minus used) returned OutOfMemory: the 39-bit heap region is 8 GiB. The SDK ignores the result, so `nn::os::AllocateMemoryBlock` handed malloc an address inside `sdk` .text and `TlsHeapCentral::Init` zeroed it. The Windows-tested titles ask for ~3 GiB and keep the rest in an alias-region root heap | `50a68a0cb` |
| 3 | boots, loads, shows frames in the top-left of the screen; host SIGSEGV after the loading screen in `vkUpdateDescriptorSetWithTemplate` (Adreno) from `ComputePipeline::Configure` | a compute shader binds 18 storage buffers; Eden kept 16 (log: `Storage buffer index 16 exceeds maximum`), so later descriptors shifted | `a767d265c` (`NUM_STORAGE_BUFFERS` 32) |
| 4 | same crash | second cap: `Skipping compute storage buffer 16 due to driver limit 16` (Adreno `maxDescriptorSetStorageBuffersDynamic` = 16; plain storage buffers are ~16M) | `a767d265c` (cap uses the plain limit) |
| 5 | 59 FPS on the autosave notice, then host SIGSEGV after more shader builds in `vkCmdBeginQuery` from `SamplesStreamer::StartCounter` (Adreno, null + 0x4dc) | open. Eden's generic occlusion query path, which Switch 1 games use on Adreno without trouble | `EDEN_NX2_NO_OCCLUSION=1` (`0d5337c52`) avoids it (run 6) |
| 6 | with `EDEN_NX2_NO_OCCLUSION=1`: two runs, 118 s and 509 s, no query crash (switch works). Both die in host allocation: `std::bad_alloc` in Dynarmic's `FastmemPatchInfo` map, and scudo `reportMapError` under `malloc` in `EmitArm64` | host out of memory (see run 7) | - |
| 7 | user played to the main menu (accessibility prompt, loading, menu), crash on reaching it. Sampled every 2 s: mappings ~7k (not `max_map_count`), app RSS 7.5 GB + 2.6 GB swapped, MemAvailable 0.6 GB, swap free 2.6 of 16 GB at death | out of memory. A 64 GiB `mmap` test binary succeeds, so overcommit is permissive; the ENOMEM comes from the vendor kernel under real memory pressure | - |
| 8 | adb-launched (`exefs_path`). Guest memfd (`RssShmem`) reached 7.4 GB within ~1 min of boot | `HostMemory::ClearBackingRegion` memset every page the kernel allocated; the game's ~9.5 GiB SetHeapSize got written in full | `411b16f8b`: guest shmem ~1.3 GB at the same point |
| 9 | with `411b16f8b`, heapprofd attached: crash at the loading screen, `Scudo ... Out of memory` on CPUCore_0. Host anon 3.3 GB + guest 3.2 GB | host anon growth (open) | - |
| 10 | crash at loading, GPUBack thread in `Scheduler::AcquireNewChunk` from `RasterizerVulkan::Clear`; anon +1.75 GB in 2 s. Log: texture cache inserted a **3D image 1920x1080x643** at 0x811a247000 (normally a 1920x1080 RT), "does not fit; using an unbacked sparse image", then `Native MME (JIT, split front) failed: start=0x32 ... instruction limit exceeded`, then `Device loss occurred!`. After device loss the Vulkan worker stops draining, command chunks pile up until OOM | front-thread macro (arm64 JIT, `EDEN_NX2_SPLIT=2` default) ran away and left Maxwell registers inconsistent (layer count 643) | open; not seen with `EDEN_NX2_MME_JIT=0` (run 11) |
| 11 | `EDEN_NX2_MME_JIT=0` (interpreter, macros on the back thread): no 643 image, no device loss, still OOM at 118 s in the Adreno driver's `calloc` on VulkanWorker. Anon climbed steadily to 4.1 GB | host anon growth (open) | - |
| 12 | heapprofd from startup (`tools/nx2/android/heapprofd.cfg`): OOM in `vkAllocateDescriptorSets` from `ResourcePool::Grow` (descriptor pools) on the Vulkan worker. heapprofd's client disconnects ~35 s in at ~0.8 GB of malloc while anon RSS was ~2 GB | host anon growth (open) | - |
| 13 | `EDEN_NX2_TICK_TRACE=1`: submitted vs completed ticks stay 2-5 apart, 1-13 chunks queued, pools only grow in the last second. Texture cache 167 MiB, buffer cache 11 MiB. Crash in Adreno `vkCmdPipelineBarrier2` at ~120 s under memory pressure | GPU keeps up: lead 1 (tick-gated pools) ruled out | - |
| 14 | heapprofd with 32 MiB sampling and no client timeout: still stops after ~40 s (and blocked the game) | heapprofd unusable here | `EDEN_NX2_ALLOC_PROFILE` instead |
| 15 | `EDEN_NX2_ALLOC_PROFILE=1`: live sampled operator new 2.6 GB. Dynarmic arm64 block metadata ~1.3 GB (`EmittedBlockInfo` copies 860 MB, `Block::Location` maps, `RegisterNewBasicBlock`), fiber stacks ~490 MB, GPU MemoryManager tables 64 MB. Flat ~4 GB RSS for 2 min on the loading screen, then +3 GB in 20 s at the next load, OOM at 13:19 | per-block hash maps; fibers zero 4 MiB each | `e44c49197`, `4a36de89e` |
| 16 | with both fixes: no crash in 6 min, anon peak 2.5 GB, RSS ~7 GB (guest 3.9 GB). Game passed the loading stage; user saw everything purple | GPU driver setting had reset to the system driver after reinstall (user's finding, unconfirmed) | user switched back to Turnip |
| 17 | user-driven, Turnip (`26.3.0-T30`, `driver_path` in config.ini; runs 1-16 were on the system driver): intro cutscene text over flat lavender, title/menus render (save menu clean, user reports flicker), in game still purple with HUD. RSS ~6 GB, MemAvailable ~1.3 GB, no crash. Rejected native shaders: LDC indexed U8 (`b82`, 14x), F2I modes (`305`, 5x: round-up and a `0x0021e100` form), VOTE.U into UR16 (`886`, 4x), unknown `d08` (3x) and `234` (2x), graphics stage 4 (1x). Skipped draws hit the main 1920x1080 target | native shader lowering gaps | LDC sizes, VOTE.U UR16-27, F2I ceil: `lower_to_maxwell.h` (this commit's parent); `d08`, `234`, stage 4, F2I `0x0021e100` open |
| 18 | `3bc9b850b`: only `d08` rejects left (3x). Crash at the main menu, GPUBack SIGSEGV in `FixedPipelineState::DynamicState::Refresh2`: topology value past Patches indexes `POLYGON_OFFSET_ENABLE_LUT`. Before it, `Dropping method 0x1180 on unbound subchannel 5` | bad topology register | `28980992a` (skip draw) |
| 19 | `a31a3088a` + topology guard: user got **in game** (character, walking), then GPUBack SIGSEGV in `SyncpointManager::Increment` from `Maxwell3D::ProcessSyncPoint`. MemAvailable ~2 GB, not OOM. Rejects left: F2I `0x0021e100` (4x), stage 4 (1x) | syncpoint id past 192 | `28980992a` (drop increment) |
| 20 | user relaunch: stuck at the main menu, 0 FPS. Log: `Channel 1 waits on semaphore 0x2104449000 (op 4, 0x28ab, now 0x28aa)`, then `vi2 present #1263 fence (1, 10419) not reached`; the guest polls syncpoint 1 forever | a semaphore release never ran | open |
| 21 | `28980992a`: in game, correct rendering (mining tunnel, DK, HUD), 15.6 FPS, RSS 6.3 GB, ~1.1-1.9 GB free. Froze on picking up the first banana: log stops at 306.8 s after `Native QMD failed: unsupported QMD version or queue`; CPUCore_2 spins at 100%, GPU threads idle. The rejected QMD's words are a mix: the version word (`0x0020ff30`) and release slots sit 6-10 words later than in a good QMD, with `2001001e/1c/1d` in words 18-22 | a QMD read before its upload finished (theory); its releases never ran, so the CPU waits forever | re-read up to ~50 ms (`native_qmd.h`), testing |
| 22 | `c5a31d62d` (QMD re-read): crashed at ~3.5 min (before the banana), GPUBack in `Maxwell3D::ProcessMacroBind`: a fourth out-of-range register value on the split GPU thread (after the 643-layer RT, topology, syncpoint id) | suspect the GPU thread split (`EDEN_NX2_SPLIT=2`, x86-tested only) | testing `EDEN_NX2_SPLIT=0` |
| 23 | `EDEN_NX2_SPLIT=0` (one GPU thread): froze again at ~143 s on `unsupported QMD version or queue`; 50 ms of re-reads didn't help. Two QMDs with the same layout: words 38-43 of a known QMD at 2-7, words 2-37 six words later (version word at 24). No garbage-register crash in its 2.5 min | a second QMD layout, not a torn read | remap and complete without a dispatch (`native_qmd.h`), testing with `SPLIT=0` |
| 24 | `9a86ee28a` (shifted QMD remap), `SPLIT=0`: stuck loading at boot. `Channel 1 waits on semaphore 0x2103c49000 (op 4, 0x1664, now 0x1663)`; no shifted QMDs this boot | deadlock, cause unknown then | release log added |
| 25 | release log: user played until destroying terrain (lots of particles), then froze; the ground was invisible. Channel 1 (3D) waits on `0x2103ae0000` for `0x503d`, now `0x4f2d`: only compute QMD +1 releases write it, 272 missing. Channel 2 (compute) waits for `==0` on `0x2160798000`, a frame handshake (3D writes 0 and 2 at +0, compute 1 and 3 at +4, then a reset) that needs channel 1 to move. No QMD errors | the 272 increments are fence callbacks behind recorded but unsubmitted work while both channels wait | `5f99ff34d` flushes when a channel starts waiting, testing |
| 26 | `5f99ff34d` (flush on wait), `SPLIT=0`: crashed ~2 min in while loading, GPU thread in `SyncpointManager::Increment` from the puller's fence increment: an out-of-range syncpoint id also without the split thread | garbage method values are not split-only | guard in `SyncpointManager` |
| 27 | `d4bfecf14` + F2I S8 (`ead698998`): user broke terrain, then abort `VK_ERROR_OUT_OF_DEVICE_MEMORY` creating an image 977272896x977272896 (3738 layers). From 145.58 s the pusher ran QMD words as methods (`Unimplemented engine 0021/60302000/0fdf4900`, unbound subchannel 5), render targets with garbage size, address and format | GPU page-table maps applied at once while unmaps of freed memory were queued in GPU order: memory freed and reallocated at the same address lost its new mapping, and the GPU read other data as command lists. Likely the source of every garbage-register crash (runs 10, 18, 19, 22, 26) | `b1f075aa1` (maps in GPU order), testing |
| 28 | `b1f075aa1`, `SPLIT=0`: crashed at 204 s (14:28:49), ~40 s after the first automated walk/punch input: GPU thread SIGSEGV in `Maxwell3D::ProcessMacroBind`. Just before: `Unmapped native inline QMD destination`, methods on unbound subchannel 6, `Unimplemented engine E000001`, puller methods 0x10-0x3F, `SEM_EXECUTE` at unmapped 0xe0a000000000. Pushbuffer dump (`nx2-runs/run28/eden_log.txt`): GPFIFO entry 1706 (0x215caf4f9c, 0x3a words) starts like commands, then holds a table of (addr, 0x21, 0x2002070x, 0x2002018x, 0, 0x4ae0) records, not methods | GPU maps in GPU order did not stop the garbage. New theory: the pushbuffer was rewritten before the GPU thread read it, i.e. the guest is told an entry was consumed (GP_GET, or a fence/syncpoint) earlier than the GPU thread actually reads it | open: check when GP_GET / entry-consumed fences are reported versus `DmaPusher` reads; copy entries at submit time as a test |
| 29 | run 28's env + `EDEN_NX2_PB_CHECK=1`, no input: 491 entries changed between submit and execution during loading (49-100 s), 565 of 3.1M by the end; the count was flat for the last ~3 min. Crashed at 334 s on the loading/in-game transition, GPU thread SIGSEGV in `Maxwell3D::ProcessMacroBind` again. Garbage started with `assert method == executing_macro + 1`: macro 0x1a4d got params at 0x1a4e then 0x1a50, after a long run of normal 0x8e0-0x8e6/0xe36 writes; then `Native MME failed ... instruction limit exceeded`, unbound subchannel 7, compute methods out of range. No dump (PB_DUMP only fires on unknown host methods) | rewritten entries are real, but none were counted in the last report 0.2 s before the garbage, so they may not be this crash's cause | `EDEN_NX2_PB_COPY` (run from submit-time snapshots, log differences) and a dump on the first macro desync, testing |
| 30 | run 29's env + `EDEN_NX2_PB_COPY=1`, user played: no crash in 358 s (stopped for the next build); 241 of 24M entries differed from submit. The diffs are the guest re-patching reused command buffers for the next frame, e.g. entry `0x210fac31f0` word 176 cycles `0c3ea000 -> 0d632000 -> 0e87a000` (triple-buffered address), entry `0x21812f51d0` words 13/20 shift by 0x30. After heavy smashing the scene turned flat lavender with the HUD on top: the game's DLSS-style pass (HMMA compute, grid 40x18, block 128) reads a 960x540 B10G11R11 image (cbuf0+0xa0) and writes the 2880x1620 A8B8G8R8 window image (cbuf0+0xa8); the program is rejected and the bilinear stand-in only knew title 4's 0x90 -> 0xa0 layout | snapshots look right (no garbage); upscaler output never written | stand-in also tries 0xa0 -> 0xa8 with format conversion, testing |
| 31 | stand-in with fixed 0xa0 -> 0xa8: purple again after smashing banana/gold piles. `Upscale fallback: no plain 2D colour input/output pair`: this dispatch had the input at 0x90 (960x540 B10G11R11), an unrelated 2048x2048 Z16 at 0xa0, output still at 0xa8. Handles move between dispatches | fixed offsets too brittle | scan cbuf0 handles: largest colour image = output, largest smaller colour image with the same aspect (within 2%) = input; title 4's 0x90 -> 0xa0 tried first |
| 32 | scan stand-in, user played ~4 min: no crash, no purple reported. adb dropped (USB) at 17:29 before the log was pulled, so the fallback's log line is unchecked | - | - |
| 33 | geometry stage accepted (`39e6f2ba4` work in progress): ground renders; destructible terrain (walls) invisible. The culling draw's GS (passthrough, SPH bit 24) failed lowering at 0x30 (`IMAD.X R1, RZ, RZ, R1, P0`) | GS draw still skipped | IMAD.X with zero product |
| 34 | GS lowers to 0x5d0, fails on F2I U16 with truncation (`0021e800`); 7 compute shaders newly rejected on b60 (a regression from a TEX LZ change, reverted). All 650 programs dumped (`EDEN_NX2_SHADER_DUMP`) | - | `tools/nx2/lowertest` checks dumps offline: 650/650 lower |
| 35 | GS lowers; pipeline fails `Read attribute Generic31Y is not implemented` | upstream `IR::IsGeneric` stopped at Generic31X; the emulated passthrough GS copies all components | `4645105e4` |
| 36 | no skipped draws: destructible terrain now drawn, but solid black (Eden-NS2 shows the same on NVIDIA). The GS draw is a culling pass (PS writes constant white; GS writes visible triangles to global memory and sets the viewport mask); the colour comes from a later pass | open | draw trace (`EDEN_NX2_DRAW_TRACE`) in progress |
| 37 | GPU split and macro JIT back on (default env minus `SPLIT=0`/`MME_JIT=0`, with `PB_COPY`): ~5 min of play, no crash, no speedup (user). Passthrough skip mask all zero, 20 generics passed. 531/531 dumped shaders lower. Abort when the user closed the app: `__cxa_rethrow` in `PresentManager::CopyToSwapchain` on the split back thread (present after the surface was gone) | split looks stable now that pushbuffer snapshots fixed the garbage commands | shutdown abort open (minor) |
| 38 | `EDEN_NX2_SSBO_BARRIER=1` + `EDEN_NX2_DRAW_TRACE=400`: terrain still black, menu wallpaper still missing (barriers are not the cause). All 400 traced events were culling draws, one per terrain chunk: indexed, colour mask 0 on both targets, depth write on (LESS); GS ssbo c0[0x410] = triangle counter (0x40 bytes), c0[0x420] = compacted u16 index buffer (`next` 0x3a98). The trace's `instances 0` is Eden's 0-based counter (1 instance) | - | longer trace |
| 39 | `EDEN_NX2_DRAW_TRACE=6000` (`nx2-runs/run39-trace/trace.txt`): 1408 culling draws, 4589 other draws, 3 dispatches in one window; no later draw's index, indirect or storage buffer points into any culling draw's counter/index buffers. Most other draws (3738) use ps 0x211ca38500 into 128x128 targets (probes/shadows?) | the colour pass for the terrain was not found in this window | open |

Top-left picture: the window images are 2880x1620 (`gsv 111 CreateNativeMemory size=0x1248000`), and
vi2 229 word 7 says 0x0437077f = 1920x1080 visible (first present: full 2880x1620). Fixed in
`a66ba9fa1`; confirmed on the phone (run 8: intro text and loading screen fill the outer display).

### Open: host memory growth (next blocker)

Budget: ~7.5-8 GB available on a fresh boot of the phone's apps; guest memory the game really uses is
~2.5-3.5 GB by the main menu. Host (non-guest) anonymous memory grows to 3-5 GB during loading
(`RssAnon` in `/proc/<pid>/status`; guest memory is `RssShmem`, the `memfd:HostMemory` mapping). An
smaps dump at 4.8 GB anon (run 8, before `411b16f8b`) showed `[anon:scudo:primary]` 3.9 GB RSS + 1.5 GB
swapped (small mallocs), `[anon:scudo:secondary]` 0.5 GB, `/dev/kgsl-3d0` 0.3 GB.

Leads, most promising first:
1. Resources that wait for GPU ticks pile up: run 10 (command chunks after a device loss) and run 12
   (`ResourcePool::Grow` for descriptor sets) both die growing pools that are only recycled once the
   master semaphore passes their tick. Check whether ticks advance during loading (log
   `master_semaphore->KnownGpuTick()` against `CurrentTick()`), and whether the pools, staging
   buffers and chunk reserve stop growing if the GPU thread waits when it gets far ahead.
2. The front-thread macro JIT runaway (run 10): reproduce with `EDEN_NX2_SPLIT_VERIFY=1`, or compare
   with `EDEN_NX2_SPLIT=1` (macros on the back, JIT still on) to split JIT from front-macro issues.
3. Shader/pipeline memory: hundreds of native shaders are lowered during loading (IR + SPIR-V + Adreno
   compiler state). `use_disk_shader_cache` and the driver pipeline cache are on.
4. Cheap mitigations to test with settings: resolution scale 0.5x (`resolution_setup=1`), the
   10GB memory layout, closing other apps first (`adb shell am kill-all` kills cached background
   processes; ask the user before running it).

heapprofd only sees malloc and its client drops after ~30 s here, so it profiled boot only. For the
growth, sample `smaps` (largest `scudo:primary` regions grow) or add counters/logging to the
suspects above.

### Separate bug: front macro runaway (run 10)

With the default `EDEN_NX2_SPLIT=2` and the arm64 macro JIT, one run hit `Native MME (JIT, split
front) failed: start=0x32, consumed=1/1, error=instruction limit exceeded` right after the texture
cache inserted a 1920x1080x643 3D image, then the device was lost. Runs 6/7 (same JIT) didn't log it,
so it is not deterministic. The arm64 JIT was fuzzed against the x86-64 JIT and the interpreter on
random programs, not on DMA/FIFO interplay or the front's register model.

Still auto-stubbed (logged once each): `ISelfController` 2400, `IApplicationProxy` 40, `IUserShimScopedObject`
300/1, `hid` 3110, `IGraphicsScaleController` 1050, several `nx2:gsv`/`nx2:vi2`/`nx2:gpu`/`nx2:syncpt`
commands. Rejected native shaders (opcode 0xd08, indexed constant load, float-to-integer conversion
mode, graphics stage 4) are logged but haven't crashed anything so far.

## Automated test runs (no tapping needed)

The user allows starting the game over adb. Scripts in `tools/nx2/android/` (settings in `env.sh`;
output in `nx2-runs/<tag>/`, git-ignored via `.git/info/exclude`):

```sh
tools/nx2/android/build_install.sh                 # build RelWithDebInfo, install with --user 0
tools/nx2/android/launch.sh EDEN_NX2_NO_OCCLUSION=1 # writes files/nx2_env.txt, force-stops, wakes, starts the game
tools/nx2/android/launch.sh -                       # same, keeping the current nx2_env.txt
tools/nx2/android/watch.sh 300 run13                # memory every 2 s until exit or 300 s, keeps screen awake, then collect
tools/nx2/android/collect.sh run13 <pid>            # log, exit reason, newest native crash symbolized, screenshots
python3 tools/nx2/android/heapprofd_top.py <trace> [pid] [count] [lib]   # heapprofd trace summary
```

- `launch.sh` uses `am start -n dev.eden.nx2.relWithDebInfo/org.yuzu.yuzu_emu.activities.EmulationActivity --es exefs_path <game folder>` (`a4e388c17`).
- The phone's screen must be on, or emulation waits for a surface on a black screen: the scripts
  send `KEYCODE_WAKEUP`. If the keyguard is up, ask the user to unlock.
- Standing authorization (user, 2026-10-30): agents may install new builds and restart the game at
  any time, even while the user is playing; no need to ask or wait for the user to finish.
- The game asks for accessibility settings on its first boot (needs an A press; the user did it once,
  later boots went straight to loading). If a run hangs on it, `adb shell input tap` on the overlay's A
  button (outer display, 2420x1080) or ask the user.
- The app is debuggable, so `adb shell run-as dev.eden.nx2.relWithDebInfo cat /proc/<pid>/smaps`
  works for memory breakdowns.
- `tools/nx2/android/press.sh [A|B|X|Y|PLUS|MINUS|UP|DOWN|LEFT|RIGHT]` taps the touch overlay on the
  outer display (`input -d 0 tap`, landscape 2420x1080 coordinates); use it for the first-boot
  accessibility prompt instead of asking the user. Not verified yet: check with a screenshot.
- `EDEN_NX2_ALLOC_PROFILE=1` writes `files/alloc_profile.txt`; pull it and run
  `python3 tools/nx2/android/alloc_profile_top.py alloc_profile.txt <unstripped lib> 8`. Symbolize
  with the lib of the same build: a rebuild changes the offsets.
- The GPU driver choice (system vs Mr. Purple Turnip) may reset after `adb install -r`; check the
  app's driver setting (or the log's `Vulkan Driver:` line) after each install.
- A 2 s acquire stall now logs the releases near the waited address (`release <addr> = <value>
  (source N)`; 1 puller, 2 puller mirror, 3 QMD add, 4 QMD max, 5 QMD write, 6 3D report).
- `launch.sh` prints `driver_path` from config.ini; empty means the system driver.
- Current `nx2_env.txt` on the phone: `EDEN_NX2_NO_OCCLUSION=1`, `EDEN_NX2_PB_COPY=1`,
  nothing else (normal play since 18:36).
- Right after `adb install`, the first `launch.sh` may land on the app's menu (`No game found in
  arguments or intent` in the log); run it again.

## Debugging a crash on the phone

1. Emulator log: the app rotates it on start, so a crashed run is usually in `eden_log.txt.old.txt`:
   `adb pull /sdcard/Android/data/dev.eden.nx2.relWithDebInfo/files/log/eden_log.txt.old.txt`.
   The log caps at 100 MB.
2. Native crash (host SIGSEGV, last log line `Unhandled SIGSEGV`): `adb logcat -b crash` stays empty on
   this phone; use `adb shell dumpsys dropbox --print data_app_native_crash`.
3. Symbolize `libyuzu-android.so` frames with the unstripped lib (check the Build ID matches):
   `$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-addr2line -f -C -i -e <lib> 0x<pc>`.
4. Guest crash: `Backtrace @ PC=` blocks in the log. Map frames to SDK symbols (needs numpy):
   `python3 tools/nx2/nso_syms.py <exefs>/sdk addr <hex pc - sdk load address>` (load addresses are in
   the log as `Module sdk loaded at 0x...`; `got <offset>` resolves PLT imports). `tools/nx2/nso_dis.py`
   calls `clang.exe` and only works on Windows; on macOS point the same logic at the NDK's `clang`.
5. Screenshot: two displays, so pass an id: `adb shell dumpsys SurfaceFlinger --display-id`, then
   `adb exec-out screencap -d <id> -p > shot.png`. The game was on the outer display.
6. `setprop wrap.<package>` is refused on this user build; use `nx2_env.txt` for env switches.

## Related forks

- **Takoroka/eden-nx2** (`https://git.eden-emu.dev/Takoroka/eden-nx2`): the source of this port. Tag
  `nx2-2026-10-29` equals `faf835afb`; branch `eden-nx2` is 3 small commits ahead (GCC build fix for
  the vi2 present stats, README and NX2_NOTES updates), so this branch should rebase onto it cleanly.
- **0x2593/Eden-NS2** (`https://github.com/0x2593/Eden-NS2`, fetched as `ns2/main`, read-only): a
  separate Switch 2 fork on the same Eden base, not the same build. No `nx2_gsv.h`; much of it is
  large patch files (`NS2.patch`, `patches/eden-ns2-dkb.patch`, 56k lines). Its README (unverified)
  lists DK Bananza v7.0.0 as playable on Windows with black destructible terrain, plus Mario Kart
  World, Pokopia and Splatoon Raiders. Its Bananza commits touch the GPU map limit, compute uploads,
  QMD release, LSE atomics, a video codec stand-in and geometry shaders. Reportedly someone applied
  a patch from it and got games booting; not checked here.
  - Scanned 2026-10-30 at `5d2d3cf0a` (all 405 changed files vs `26a03d247`, 117k added lines,
    including both patch files): nothing malicious found. No network, process-launch, registry,
    dynamic-loading or encoded payloads in the C++; the 5 PNGs are plain images; build changes only
    add sources plus one dependency, `libgav1` from `chromium.googlesource.com/codecs/libgav1` pinned
    to `c1deec657b32`. `tools/ns2` scripts are local Windows dev tooling (build, launch, keypresses,
    screenshots, a gdbstub client on 127.0.0.1). `guest_hooks.cpp` / `dlss_trace.cpp` are env-gated
    debugging aids. Rescan anything newer before building it.
  - Overlap with this branch: it also sets `NUM_STORAGE_BUFFERS = 32` and lets Switch 2 heaps grow
    (it scales the address-space regions with the 12 GB layout instead of a fixed 16 GiB). It has no
    fix for the Qualcomm dynamic storage buffer cap, the vi2 crop, or the Adreno `vkCmdBeginQuery`
    crash (its query cache changes are tracing only; `vk_query_cache.cpp` is untouched).
  - Not portable as one patch: different service layout (no `nx2_gsv.h`). Port single fixes.
  - Fetched as remote `ns2` (promisor, `pushurl` disabled) so its blobs load on demand.

## Sending changes upstream

Repo policy (`AGENTS.md`, `docs/policies/AI.md`): the human submits and pushes, never an agent; AI
use must be disclosed in every commit message and the PR description (`Assisted-by:` trailer);
`Co-authored-by:` tags for AI are not allowed, and commit messages and PR text must be written by
the submitter. The commits up to `babcf2a08` still carry `Co-Authored-By: Claude` trailers and
must be reworded before anything is submitted. Pull requests to Takoroka's fork need an account
on git.eden-emu.dev and a fork there.

## Known limits and ideas

- Android may kill the app under memory pressure with the 12GB layout on a 16 GB phone (RSS was
  ~3.6 GB during loading).
- Loose titles don't appear in the game list; launch them from the folder button each time.
- Upstream's Maxwell `MacroJITx64Impl` is still x86-only (not needed for Switch 2 titles).
- `EDEN_NX2_SPLIT=2` (macros on the front GPU thread) now compiles on arm64 but is untested there.
- macOS build of this branch: possible (M5 Pro, 24 GB), not attempted.
