> [!IMPORTANT]
> **AI disclaimer:** most of this fork's Android port (the code, docs and tooling on top of Takoroka's eden-nx2)
> was written with the help of an AI coding assistant (Claude). It was then reviewed and tested by hand, on a single phone.
> Treat it as experimental code. Bugs that a careful human author would have caught can still be in it.

> [!WARNING]
> **This is an unofficial, experimental fork. It is not Eden, and the Eden team does not support it.**
> Please do not report problems from this build to the Eden project or Takoroka.

## Compatibility

| Game | Version | Status |
| --- | --- | --- |
| Donkey Kong Bananza (`040074A01BF12000`) | update 458752 | **Playable.** Terrain renders. Low frame rate, long loading. No main menu wallpaper. |
| Pokémon Pokopia | 2.0.1 | Untested |
| Other Switch 2 games | - | Untested |

| Device | GPU driver | Status |
| --- | --- | --- |
| Honor foldable, Snapdragon SM8850 (Adreno 840), 16 GB RAM | Mr. Purple Turnip T30 | Tested |
| Same phone | Qualcomm 512.842.44 | Boots, less tested |
| Phones with less than 16 GB RAM | - | Likely killed by Android while loading (the app uses about 5 GB including swap) |
| Mali, PowerVR or other non-Adreno GPUs | - | Untested |

## Eden NX2 for Android

This fork brings [Takoroka's eden-nx2](https://git.eden-emu.dev/Takoroka/eden-nx2) (experimental Switch 2
support, Windows-only) to Eden's Android app. Takoroka wrote the Switch 2 work underneath. This fork adds
the Android port, the loose dump loader, and fixes found while running Donkey Kong Bananza on a Snapdragon phone.

Since v0.2 this fork also follows [Eden-NS2](https://github.com/0x2593/Eden-NS2) (0x2593's Switch 2 fork) and ports
its fixes one at a time. The DK Bananza terrain fix comes from Eden-NS2.

### Install

- Get the APK from this repo's **Releases** page only, and check its SHA-256 against the one in the release notes.
- It installs as `dev.eden.nx2` ("Eden NX2"), alongside the official Eden app, and keeps its own settings and saves.
- It needs an arm64 device on Android 13 or newer. A phone with 16 GB of RAM is realistic for Switch 2 titles.

### Loading a Switch 2 dump

Loose (extracted) dumps boot straight from their `exefs/main`. Dumps in NSP, XCI or other container formats are not supported for Switch 2 titles.

1. Tap the folder button in the games list, then choose **Launch ExeFS game folder**.
2. When asked, grant **All files access**. The app needs it to read the files next to `main`.
3. Pick the game's folder. The app searches its subfolders for `exefs/main`. These layouts work:
   - `Game/exefs/` + `Game/romfs/`
   - the `exefs/` folder itself
   - `Game/base/`, `Game/updates/<version>/`, `Game/dlc/<title id>/<version>/romfs`
4. If the app finds several (for example base and an update), pick one from the list. The newest update is listed first.
5. Loading sits on a black screen for a while. That alone is not a hang.

Settings that work, and since v0.2 the app's defaults: CPU backend **Dynarmic**, GPU mode **Fast**, async
presentation **off**, resolution **0.5x**, asynchronous shaders **on**. Switch 2 games switch to the memory layout
**12GB (Switch 2, Unsafe)** on their own. Settings you changed yourself keep your value. Keys and firmware are not
needed for loose Switch 2 titles.

### Troubleshooting

**The app closes on its own, with no error.** That is almost always Android's background (low-memory) killer, not a
crash: DK Bananza needs about 5 GB, RAM plus swap.
- Keep the game in front. Switching to another app, even briefly, makes Eden NX2 a background app, and it is the first
  thing Android kills.
- Close other apps before launching, especially browsers, games and social apps.
- Turn off battery optimisation for Eden NX2 (Settings → Apps → Eden NX2 → Battery → Unrestricted). Some phones
  also have their own "app freezer" or "smart memory" feature; exclude Eden NX2 there too.
- Phones with less than 16 GB of RAM will likely not cope.

**Black screen at 0 FPS after launching.**
- **Wait.** Loading takes 2-3 minutes on a black screen before the first frame. That alone is not a hang.
- **Keep the screen on and the app in front.** With the screen off or the app in the background, emulation waits.
- **First boot:** DK Bananza asks for accessibility settings once. Press A on the on-screen controller.
- **Settings:** memory layout **12GB (Switch 2, Unsafe)** and CPU backend **Dynarmic**. NCE does not work with the
  big layouts.
- **Wrong folder:** pick the folder that holds `exefs/` and `romfs/` (or a parent the app can search). Without
  `romfs` the game can't load.
- **GPU driver:** this build is tested with Mr. Purple Turnip T30 on Adreno 840. Try Turnip if the stock driver
  hangs. Non-Adreno GPUs are untested.
- If it still hangs, share `Android/data/dev.eden.nx2/files/log/eden_log.txt` and your phone model, RAM and driver.

**Low frame rate.** Check that resolution is 0.5x and asynchronous shaders are on (the defaults). Close other apps, and let the phone cool
down: it throttles when hot.

### Be careful about

- **Legal:** only use games, keys and firmware dumped from hardware you own. None are included, and none will be provided.
  Don't ask for them.
- **Signing key:** the APK is signed with the public test key shipped in Eden's source. Anyone can sign an APK with that key,
  so another "Eden NX2" APK could install over this one. Only install builds from this repo, and check the checksum.
- **All files access:** this permission lets the app read and write all of shared storage. It is only needed for loose
  dumps, and you can revoke it in Android's settings afterwards.
- **Memory:** the 10GB and 12GB layouts are unsafe. Android may kill the app, or other apps, under memory pressure.
  Phones with less RAM will likely not cope.
- **Heat and battery:** expect the phone to get hot and the battery to drain fast. Watch for thermal throttling.
- **Saves:** crashes can happen at any time. Back up saves you care about, under `Android/data/dev.eden.nx2/files/`.
- **Tested on one phone and one game:** an Honor foldable (Snapdragon SM8850, Adreno 840) with Turnip and the Qualcomm driver,
  and only Donkey Kong Bananza. Other Switch 2 games, GPUs and drivers are untested.
- **Diagnostics:** the `EDEN_NX2_*` switches in [NX2_NOTES.md](NX2_NOTES.md) are debugging aids. Some slow the
  game down a lot or fill storage with logs.

### Known problems

- **Crashes**, including a crash when closing the app while the GPU thread split is on, and an occasional crash with async presentation on (keep it off).
- **Memory:** the app needs about 5 GB (RAM plus swap) in DK Bananza. Phones with less RAM will likely see the app killed while loading.
- **Purple screen:** fixed for the cases seen so far in v0.2, but report it if it comes back.
- **No main menu wallpaper**, and Joy-Con colours are wrong in the UI.
- **Low frame rate** in DK Bananza on Turnip.
- **Long black loading screens** before the first frame.

### How it works

Takoroka's eden-nx2 does the Switch 2 groundwork: the new services (`nx2:gsv`, vi2), the newer GPU's command and
shader formats, and the loader. This section covers what this fork changed to get it running on Android.

**CPU and memory**
- **12 GB guest memory:** Switch 2 games need more memory than Switch 1 games. The fork adds 10 GB and 12 GB memory
  layouts, gives Switch 2 processes a 16 GiB heap region, and lets heaps grow past the old 8 GiB limit. Switch 2
  games switch to the 12 GB layout on their own.
- **Fitting in a phone's RAM:** the first boots ran out of memory. A sampling allocation profiler
  (`EDEN_NX2_ALLOC_PROFILE`) showed where the memory went, and three fixes followed:
  - Dynarmic's per-block metadata moved from hash maps to vectors. It was over 1 GiB.
  - Guest thread stacks are no longer zeroed. Zeroing touched 4 MiB per thread, about 490 MiB in total.
  - Clearing guest memory now punches holes in the backing memory file instead of writing zeros, so clearing a
    heap no longer allocates all of it.
- **Dynarmic, not NCE:** layouts above 4 GB need Dynarmic.

**GPU command processing**
- **Macro JIT on arm64:** the GPU's macro programs (small programs the game uploads to the GPU, run here on the
  CPU) only had an x86-64 JIT, so Android used the slow interpreter. The fork adds an arm64 backend built on
  oaknut. It was fuzzed against the x86-64 JIT and the interpreter with 100,000 random programs and 0 mismatches,
  and runs about 5x faster than the interpreter (`tools/nx2/jit_fuzz/`).
- **Pushbuffer snapshots (`EDEN_NX2_PB_COPY`):** the game reuses and edits its GPU command buffers for the next
  frame, sometimes before the emulated GPU has read them. Eden then ran garbage as commands and crashed. GPU command
  lists are now copied when they are submitted. In v0.1 this was only on with `nx2_env.txt`; since v0.2 it is on
  by default for Switch 2 games.
- **GPU memory maps in order:** maps and unmaps on the GPU's page table are applied in the order the GPU sees them.
  Before, memory that was freed and then reused at the same address could lose its new mapping.
- **Stalled channels:** pending GPU work is flushed when a channel starts waiting. Without that, the 3D and compute
  channels could each wait on a fence the other had not submitted yet.

**Shaders**
- **Native shader lowering:** the newer GPU's instructions are lowered to ones Eden's shader recompiler
  understands. The fork adds several missing forms (LDC, VOTE, MUFU, HSETP2, F2I, IMAD.X, LEA.HI and more).
  `tools/nx2/lowertest` checks a folder of dumped shaders offline, and all 531-650 dumped DK Bananza shaders lower.
- **Up to 32 storage buffers per stage:** Switch 2 compute shaders bind more than Eden's old limit of 16 (DK Bananza
  binds 18). On Adreno and Mali the limit uses the plain, not the dynamic, storage buffer count.

**The DLSS pass (purple screen)**
- DK Bananza upscales with a DLSS-style pass: a compute shader that runs a neural network on the Switch 2 GPU's
  tensor cores. It takes a low-resolution frame (for example 960x540) and writes the full window image (2880x1620).
  Emulating those tensor-core instructions would take the host driver minutes to compile and seconds per frame, so
  the shader is rejected.
- In its place, a **bilinear stand-in** resizes the input image straight into the output, with a format conversion.
  You lose the network's sharpening, but you get a picture.
- The purple screen appears when the stand-in can't find those images. v0.1 scanned the shader's constant buffer
  for image handles, which only worked by luck. DK Bananza also has a second upscaler program that renders at
  dynamic resolution (about 1396x785 into 1920x1080). Since v0.2 the stand-in takes the images from the program's own
  texture and surface instructions, honours the dynamic-resolution crop, and skips the pass instead of failing when it
  finds nothing.

**Destructible terrain (fixed in v0.2)**
1. **Invisible to drawn:** the terrain runs a culling pass through a geometry shader first. Eden skipped that draw
   because the shader would not lower. The fork ported native geometry shader support from the Eden-NS2 fork and
   added IMAD.X. It also fixed an upstream bug where only the X component of Generic31 counted as a generic
   attribute.
2. **Drawn but black:** the culling pass writes each terrain chunk's indirect draw record to GPU memory, and the
   game then passes those words back to the GPU as command-list parameters. Eden read them from stale guest memory
   (or from the pushbuffer snapshot taken at submit time), so the draws got the wrong arguments. Following
   Eden-NS2, pages the GPU writes are now tracked, and command words on them are flushed from the host GPU before
   they run (`EDEN_NX2_PBFLUSH`). A related Eden-NS2 fix does the stores of the program that resets those records.
3. **Keeping it fast:** a flush has to wait for the GPU thread split's back half to catch up. Waiting for every
   such command took DK Bananza to 2.5 FPS. Now a page flushed since the last compute or copy work is read without
   waiting again (`EDEN_NX2_PBFLUSH_EPOCH`), which removes about 95% of the waits.

**Android app**
- Loose dumps launch by real path through All files access, because content URIs can't reach the files next to
  `exefs/main`. DLC is loaded from a `dlc/<title id>/<version>/romfs` folder next to the game.
- `EDEN_NX2_*` switches are read from `files/nx2_env.txt` at app start, since Android apps have no environment
  variables you can set.

More detail and the full run log: [NX2_ANDROID.md](NX2_ANDROID.md). Licence: GPLv3 or later, same as Eden.
Every release is built from the tagged commit in this repo.

---

<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->
<!-- lang: en-GB -->

<h1 align="center">
  <br>
  <a href="https://git.eden-emu.dev/eden-emu/eden"><img src="./dist/qt_themes/default/icons/256x256/eden.png" alt="Eden" width="200"></a>
  <br>
  <b>Eden</b>
  <br>
</h1>

<h4 align="center"><b>Eden</b> is a free and open-source (FOSS) Switch 1 emulator started by developer Camille LaVey.
<br>
Written in C++, with builds for Windows, Linux, macOS, Android, FreeBSD and more.
</h4>

<p align="center">
    </a>
    <a href="https://discord.gg/HstXbPch7X">
        <img src="https://img.shields.io/discord/1367654015269339267?color=5865F2&label=Eden&logo=discord&logoColor=white"
            alt="Discord">
    </a>
    <a href="https://stt.gg/qKgFEAbH">
        <img src="https://img.shields.io/revolt/invite/qKgFEAbH?color=d61f3a&label=Stoat"
            alt="Stoat">
    </a>
</p>

<p align="center">
  <a href="#compatibility">Compatibility</a> |
  <a href="#development">Development</a> |
  <a href="#building">Building</a> |
  <a href="#download">Download</a> |
  <a href="#support">Support</a> |
  <a href="#license">License</a>
</p>

## Compatibility

The emulator is capable of running most commercial games at full speed, provided you meet the necessary hardware requirements.

A list of supported games will be available in future. Please be patient.

Check out our [website](https://eden-emu.dev) for the latest news on exciting features, monthly progress reports, and more!

[![Packaging status](https://repology.org/badge/vertical-allrepos/eden-emulator.svg)](https://repology.org/project/eden-emulator/versions)

## Contribute

To contribute to Eden; be it financially, code, bug reports, or otherwise, see our [Contributing guidelines](./CONTRIBUTING.md).

## Documentation

We have a user manual! See our [User Handbook](./docs/user/README.md).

## Building

See the [General Build Guide](docs/Build.md)

For information on provided development tooling, see the [Tools directory](./tools)

## Download

You can download the latest releases from [our release page](https://git.eden-emu.dev/eden-emu/eden/releases).

Save us some bandwidth! We have [mirrors available](./docs/user/ThirdParty.md#mirrors) as well.

## License

Eden is licensed under the GPLv3 (or any later version). Refer to the [LICENSE.txt](https://git.eden-emu.dev/eden-emu/eden/src/branch/master/LICENSE.txt) file.

## Special thanks

Super special thanks to Cloudflare for preventing the git server from blowing up.

- Yuzu
- Ryujinx
- Sudachi
- Citron
- Torzu
- Suyu
- Ryubing

And everyone who continues or had contributed to the project! <3
