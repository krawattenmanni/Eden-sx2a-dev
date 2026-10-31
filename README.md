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
