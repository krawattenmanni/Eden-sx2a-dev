# Eden NX2 on Android: developer notes

Android port of Switch 2 emulation on top of [Eden-NS2](https://github.com/0x2593/Eden-NS2). This branch is
Eden-NS2's history plus this fork's commits; the user-facing overview is in [README.md](README.md). Versions up to
v0.2 were based on Takoroka's eden-nx2 instead.

## Following Eden-NS2

```sh
git remote add ns2 https://github.com/0x2593/Eden-NS2.git
git fetch ns2
git log --oneline HEAD..ns2/main      # read each new commit first
git merge ns2/main
```

- Merge, never rebase onto Eden-NS2, so the next merge stays easy.
- `README.md`: this fork's section sits on top of Eden-NS2's README. On a conflict keep ours at the top and take
  Eden-NS2's text below the divider.
- Likely conflict spots: `src/video_core/dma_pusher.cpp` (`PBFLUSH_EPOCH` in `FlushGpuWrittenCommands`),
  `vk_rasterizer.cpp` (topology guard, failed-pipeline path), `maxwell_3d.cpp`, `deconstructed_rom_directory.cpp`
  (loose DLC hook), the `0x361` case in `lower_to_maxwell.h` (multi-channel bindless 2D samples; drop ours once
  Eden-NS2 handles that form), and `src/core/arm/nce/visitor_base.h`, which must match every entry in dynarmic's A64
  `a64.inc` / `impl.h` (a mismatch fails as "constexpr variable 'arg_masks' must be initialized" in
  `interpreter_visitor.cpp`).
- Never edit `patches/*.patch` or `NS2.patch`.
- Android builds with clang and `-Werror`; Eden-NS2 builds with GCC/MinGW, so new Eden-NS2 code can break the Android
  build. `src/CMakeLists.txt` keeps `logical-op-parentheses` and `sign-compare` non-fatal on Android; prefer a flag
  there over editing Eden-NS2's files.

## What this fork changes

Android app:
- 10GB / 12GB memory layouts in the UI; Switch 2 defaults (0.5x, async shaders, Dynarmic, no async presentation).
- Loose ExeFS launch by real path ("Launch ExeFS game folder", `exefs_path` intent extra), with a chooser when a folder
  holds several dumps; loose DLC from `dlc/<title id>/<version>/romfs` up to two levels above the game.
- `files/nx2_env.txt`: `KEY=VALUE` lines become environment variables at app start (any `EDEN_*` switch).
- Bundled Mr. Purple Turnip T30 (`src/android/app/src/main/assets/gpu_drivers/`), selected once on Adreno devices
  that have no driver chosen.
- Swiping the app from recents stops emulation and ends the process; presenting after the surface is gone drops the
  frame instead of aborting.

Core and GPU:
- NCE visitor stubs for Eden-NS2's A64 decoder entries (Android build).
- arm64 (oaknut) backend for the Turing macro JIT, with `tools/nx2/jit_fuzz` (0 mismatches against the interpreter).
- `Nx2Program()`: Eden-NS2 marks Switch 2 programs with `g_native_channels`; the pushbuffer flush, GPU-written page
  tracking and the switches below also key on it.
- `EDEN_NX2_PBFLUSH_EPOCH` (default on): pages flushed since the last non-3D writer method are read without draining
  the GPU thread split again. `EDEN_NX2_PBFLUSH_STATS=1` logs counters every 5 s; `EDEN_NX2_PBFLUSH_MAX` limits the
  flushed entry size.
- `EDEN_NX2_NO_OCCLUSION` (default on): no host occlusion queries (Adreno crash in `vkCmdBeginQuery`).
- Qualcomm/ARM storage buffer cap uses the plain limit; storage CAS write helper defined whenever used (stock driver).
- Draws whose pipeline failed to build are dropped (logged once per shader pair); draws withheld for an async build are
  not counted as failed work; methods for unbound subchannels are dropped.
- Generic31Y/Z/W are generic attributes; multi-channel bindless 2D samples lower.
- Out-of-range topology and syncpoint ids are ignored.
- Dynarmic arm64 per-block info in vectors; fiber stacks left uninitialized.
- Faster video frame output (integer BT.709 conversion, reused buffers), 6 AV1 threads; `EDEN_NX2_VIDEO_STATS=1`.
- Diagnostics: `EDEN_NX2_ALLOC_PROFILE`, `EDEN_NX2_TICK_TRACE`, `EDEN_NX2_SHADER_DUMP=<dir>` (checked offline with
  `tools/nx2/lowertest`).

Switches that are on by default for Switch 2 programs can be turned off with `=0` in `nx2_env.txt`.

## Build

Android SDK with `cmake;3.31.5` and `ndk;28.2.13676358`, JDK 17, glslang.

```sh
sh .ci/android/build.sh -t standard -b RelWithDebInfo
```

- Eden-NS2's libgav1 comes through CMake `FetchContent`, so the first configure needs network.
- Release builds should map source paths out of the binary:
  `M=-ffile-prefix-map=$PWD/=; sh .ci/android/build.sh -t standard -b Release "-DCMAKE_C_FLAGS=$M" "-DCMAKE_CXX_FLAGS=$M"`
- `tools/nx2/android/` holds phone scripts (build/install, launch over adb, memory watch, log and crash collection).

## Debugging on a phone

- Emulator log: `Android/data/dev.eden.nx2/files/log/eden_log.txt` (the previous run is `eden_log.txt.old.txt`).
- Native crashes: `adb shell dumpsys dropbox --print data_app_native_crash`; symbolize `libyuzu-android.so` frames
  with the unstripped library of the same build (`llvm-addr2line -f -C -i -e <lib> 0x<pc>`).
- Guest crashes: `Backtrace @ PC=` blocks in the log; `tools/nx2/nso_syms.py` maps addresses to SDK symbols.
