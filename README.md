> [!IMPORTANT]
> **AI disclaimer:** most of this fork's Android port (the code, docs and tooling on top of Takoroka's eden-nx2)
> was written with the help of an AI coding assistant (Claude). It was then reviewed and tested by hand, on a single phone.
> Treat it as experimental code. Bugs that a careful human author would have caught can still be in it.

> [!WARNING]
> **This is an unofficial, experimental fork. It is not Eden, and the Eden team does not support it.**
> Please do not report problems from this build to the Eden project or Takoroka.

## Eden NX2 for Android

This fork brings [Takoroka's eden-nx2](https://git.eden-emu.dev/Takoroka/eden-nx2) (experimental Switch 2
support, Windows-only) to Eden's Android app. Takoroka wrote the Switch 2 work underneath. This fork adds
the Android port, the loose dump loader, and fixes found while running Donkey Kong Bananza on a Snapdragon phone.

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

Settings that work: memory layout **12GB (Switch 2, Unsafe)**, CPU backend **Dynarmic**, GPU mode **Fast**,
async presentation **off** (see [NX2_ANDROID.md](NX2_ANDROID.md) for how to set it). Keys and firmware are not
needed for loose Switch 2 titles.

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
- **Destructible terrain draws solid black**: walls and much of the ground in DK Bananza. This is the main open bug.
- **Purple screen:** sometimes the game switches to a purple screen, usually after heavy scenes, and only restarting the app fixes it.
- **No main menu wallpaper**, and Joy-Con colours are wrong in the UI.
- **Low frame rate:** about 12-16 FPS in DK Bananza on Turnip.
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
  lists are now copied when they are submitted.
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
- The purple screen appears when the stand-in can't find those images. Their handles move between dispatches, so
  fixed offsets broke. It now scans the shader's constant buffer: the largest colour image is the output, and the
  largest smaller colour image with the same aspect ratio is the input. This fixed most purple screens, but some
  still happen.

**Destructible terrain (still black)**

The terrain isn't fixed yet. Here is how far it got:
1. **Invisible to drawn:** the terrain runs a culling pass through a geometry shader first. Eden skipped that draw
   because the shader would not lower. The fork ported native geometry shader support from the Eden-NS2 fork and
   added IMAD.X. It also fixed an upstream bug where only the X component of Generic31 counted as a generic
   attribute. The pass now runs, and the terrain is drawn.
2. **Drawn but black:** the culling pass writes per-chunk triangle counters and compacted index buffers to storage
   buffers, but only to decide what is visible. The colour comes from a later pass.
3. **Ruled out so far:**
   - shader rejections
   - skipped draws
   - the passthrough attribute mask
   - geometry shader storage tracking
   - missing storage barriers (`EDEN_NX2_SSBO_BARRIER`)
4. **Draw trace (`EDEN_NX2_DRAW_TRACE`):** across 6,000 draws, no later draw read the culling buffers. Next is
   finding what consumes them: maybe a later frame, or another channel. Or the data stays in host GPU memory and is
   never synced back to guest memory. The Eden-NS2 fork shows the same black terrain on NVIDIA PCs, so it is likely
   not Adreno-specific.

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
