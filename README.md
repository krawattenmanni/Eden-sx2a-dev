> [!IMPORTANT]
> **AI disclaimer:** most of this fork's Android port (the code, docs and tooling on top of Eden-NS2) was written
> with the help of an AI coding assistant (Claude). It was then reviewed and tested by hand, on a single phone.
> Treat it as experimental code. Bugs that a careful human author would have caught can still be in it.

> [!WARNING]
> **This is an unofficial, experimental fork. It is not Eden, and the Eden team does not support it.**
> Please do not report problems from this build to the Eden project, Eden-NS2 or Takoroka.

## Eden NX2 for Android

Android build of Switch 2 emulation. Since v0.2.1 it is built on [Eden-NS2](https://github.com/0x2593/Eden-NS2)
(0x2593's Switch 2 fork of Eden) and follows it with ordinary merges; this fork adds the Android app layer and the
fixes needed on phones. Versions up to v0.2 were built on [Takoroka's eden-nx2](https://git.eden-emu.dev/Takoroka/eden-nx2).
Eden-NS2's own README follows below this section.

**Best-running game so far: Pokémon Pokopia, playable at about 20 FPS** on a Snapdragon SM8850 phone
(Adreno 840, 16 GB RAM) with the bundled Turnip driver.

## Compatibility

| Game | Version | Status |
| --- | --- | --- |
| Pokémon Pokopia (`0400A3A01EE4C000`) | update 327680 | **Playable**, about 20 FPS most of the time. First load takes about 4 minutes |
| Donkey Kong Bananza (`040074A01BF12000`) | update 458752 | **Playable** on Turnip, terrain drawn. About 5-20 FPS, long loading |
| Nintendo Switch 2 Welcome Tour (`040057C02159A000`) | - | Boots; can crash after character creation |
| Mario Kart World (`0400C3F00006E000`) | 1.0.0 (no update) | **Does not boot** (black screen). Eden-NS2 was tested with update 1.8.0, which is untested here |
| Other Switch 2 games | - | Untested on Android. Reports welcome |

| Device | GPU driver | Status |
| --- | --- | --- |
| Honor foldable, Snapdragon SM8850 (Adreno 840), 16 GB RAM | Mr. Purple Turnip T30 (bundled) | Tested |
| Same phone | Qualcomm 512.842.44 (stock) | Runs, but some draws fail (missing ground). Use Turnip |
| Phones with less than 16 GB RAM | - | Likely killed by Android while loading |
| Mali, PowerVR or other non-Adreno GPUs | - | Untested |

### Install

- Get the APK from this repo's **Releases** page only, and check its SHA-256 against the one in the release notes.
- It installs as `dev.eden.nx2` ("Eden NX2A"), alongside the official Eden app, and keeps its own settings and saves.
  v0.2.1 installs over earlier Eden NX2 versions and keeps your settings.
- It needs an arm64 device on Android 13 or newer. **RAM:** 16 GB is recommended; 12 GB may work for lighter games.
  DK Bananza peaks at about 6 GB for the app.
- **GPU driver:** on Adreno GPUs the app ships Mr. Purple's Turnip T30 (Mesa, MIT licence, 3.7 MB) and selects it on
  first start if you have not picked a driver yourself. You can switch back to the system driver in the driver manager.

### Loading a Switch 2 dump

Loose (extracted) dumps boot straight from their `exefs/main`. Dumps in NSP, XCI or other container formats are not
supported for Switch 2 titles.

1. Tap the folder button in the games list, then choose **Launch ExeFS game folder**.
2. When asked, grant **All files access**. The app needs it to read the files next to `main`.
3. Pick the game's folder. The app searches its subfolders for `exefs/main`. These layouts work:
   - `Game/exefs/` + `Game/romfs/`
   - the `exefs/` folder itself
   - `Game/base/`, `Game/updates/<version>/`, `Game/dlc/<title id>/<version>/romfs`
4. If the app finds several (for example base and an update), pick one from the list. The newest update is listed first.
5. Loading sits on a black screen for a while. That alone is not a hang.

Default settings: CPU backend **Dynarmic**, GPU mode **Fast**, async presentation **off**, resolution **0.5x**,
asynchronous shaders **on**. Switch 2 games switch to the memory layout **12GB (Switch 2, Unsafe)** on their own.
Keys and firmware are not needed for loose Switch 2 titles.

The app needs no extra configuration: the GPU driver and the switches tested on the phone are built in. Switch 2 games
run the GPU work on two threads (`EDEN_NX2_SPLIT=2`) with the faster command-list flush, which is quicker on phones
than one thread. To try other values, put `KEY=VALUE` lines in `Android/data/dev.eden.nx2/files/nx2_env.txt`
(for example `EDEN_NX2_SPLIT=0`); the file is read at app start.

### Troubleshooting

**The app closes on its own, with no error.** Usually Android's low-memory killer, not a crash.
- Keep the game in front, close other apps first, and set battery usage for Eden NX2A to Unrestricted. Exclude it from
  any "app freezer" or "smart memory" feature your phone has.
- Phones with less than 16 GB of RAM will likely not cope.

**Black screen after launching.**
- **Wait.** Loading takes a few minutes on a black screen before the first frame.
- **Keep the screen on and the app in front.** With the screen off or the app in the background, emulation waits.
- **First boot:** DK Bananza asks for accessibility settings once. Press A on the on-screen controller.
- **Wrong folder:** pick the folder that holds `exefs/` and `romfs/` (or a parent the app can search).
- **Driver:** check that the Turnip driver is selected (Settings → GPU driver manager).
- If it still hangs, share `Android/data/dev.eden.nx2/files/log/eden_log.txt` and your phone model, RAM and driver.

**Low frame rate.** Keep resolution 0.5x and asynchronous shaders on, close other apps, and let the phone cool down.
Skip in-game videos where the game allows it: they play at the game's frame rate.

**A game reaches a black screen and nothing happens for many minutes.** Check the dump: Switch 2 dumps with damaged
program files (`main`, `sdk`, `subsdk0` that fail to decompress) hang without an error. The log then shows
`assert r == int(nso_header.segments[i].size)` near the start.

**Closing the game.** Swiping Eden NX2A away from recent apps now ends the game and the app. Use that before starting a
different game if a launch misbehaves.

### Be careful about

- **Legal:** only use games, keys and firmware dumped from hardware you own. None are included, and none will be provided.
- **Signing key:** the APK is signed with the public test key shipped in Eden's source. Anyone can sign an APK with that
  key, so another "Eden NX2A" APK could install over this one. Only install builds from this repo, and check the checksum.
- **All files access:** lets the app read and write all of shared storage. It is only needed for loose dumps; you can
  revoke it afterwards.
- **Memory:** the 10GB and 12GB layouts are unsafe. Android may kill the app, or other apps, under memory pressure.
- **Heat and battery:** expect the phone to get hot and the battery to drain fast.
- **Saves:** crashes can happen. Back up saves you care about, under `Android/data/dev.eden.nx2/files/`.
- **Diagnostics:** the `EDEN_NX2_*` / `EDEN_NATIVE_*` switches are debugging aids. Some slow the game down a lot or fill
  storage with logs.

### Known problems

- **Low frame rate**: about 5-20 FPS in DK Bananza, around 20 FPS in Pokopia; being investigated.
- **Mario Kart World 1.0.0** does not boot. Newer updates are untested.
- **First loads** take minutes while shaders are built; later loads are faster.
- **Stock Qualcomm driver:** two pipelines fail to build, so parts of the scene (DK Bananza's ground) are missing.
- **Videos** may play slowly.
- **Memory:** about 4-5 GB (RAM plus swap) in DK Bananza.
- **Long black loading screens** before the first frame.

### What this fork adds on top of Eden-NS2

- **Android app:** 10 GB and 12 GB memory layouts, launching loose ExeFS dumps by real path, loose DLC
  (`dlc/<title id>/<version>/romfs`), `EDEN_*` switches from `files/nx2_env.txt`, Switch 2 defaults, the bundled Turnip
  driver, and ending the game when the app is swiped away.
- **arm64 macro JIT:** the GPU's macro programs had only an x86-64 JIT; an arm64 backend (oaknut) runs them about 5x
  faster than the interpreter. Fuzzed against the interpreter with 0 mismatches (`tools/nx2/jit_fuzz/`).
- **Adreno fixes:** the storage buffer limit on Qualcomm drivers, no host occlusion queries for Switch 2 games (they
  crashed the driver), the storage CAS helper the stock driver needs, dropping draws whose pipeline fails to build, and
  dropping methods sent to unbound subchannels.
- **Memory:** Dynarmic's arm64 per-block metadata in vectors (it was over 1 GiB) and guest thread stacks left
  uninitialized (about 490 MiB saved).
- **Pushbuffer flush:** Eden-NS2's flush of GPU-written command words (DK Bananza's destructible terrain) is turned on for
  Switch 2 games, and skips the waits for pages already flushed since the last compute or copy work
  (`EDEN_NX2_PBFLUSH_EPOCH`). Since v0.2.1 it also flushes the rest of a command list's GPU-written pages ahead, with
  one host GPU wait (`EDEN_NX2_PBFLUSH_AHEAD`, `EDEN_NX2_PBFLUSH_BATCH`).
- **GPU thread split on phones:** fewer front-thread waits (per-call-site drain counts, acquires and binds that skip the
  drain, `EDEN_NX2_DMAREAD_EPOCH`), so the split is on by default on Android too.
- **Fixes:** a crash on unmapped texture buffers (Pokopia), and the first steps of Mario Kart World 1.0.0's older SDK
  (graphics boot flags, GPU session, display interface version 1).
- **Shaders and video:** a multi-channel bindless 2D sample form in the shader lowering (Welcome Tour), Generic31Y/Z/W
  attributes, and faster video frame output.

Licence: GPLv3 or later, same as Eden. Every release is built from the tagged commit in this repo.
More detail: [NX2_ANDROID.md](NX2_ANDROID.md).

---

# Eden NS2

A fork of the [Eden](https://git.eden-emu.dev/eden-emu/eden) Nintendo Switch emulator that runs
Nintendo Switch 2 games.

![Donkey Kong Bananza gameplay](docs/ns2/screenshots/bananza-cave.png)

![Mario Kart World title screen](docs/ns2/screenshots/mario-kart-world-title.png)

![Pokémon Pokopia gameplay](docs/ns2/screenshots/pokopia-gameplay.png)

## Status

| Game | State |
| --- | --- |
| Donkey Kong Bananza (`040074A01BF12000`, v7.0.0) | Playable; slow in some areas |
| Mario Kart World (`0400C3F00006E000`, v1.8.0) | Playable in Free Roam and Grand Prix |
| Pokémon Pokopia (`0400A3A01EE4C000`) | Playable; stable 60 FPS |
| Splatoon Raiders (`0400380020056000`) | Playable |
| Nintendo Switch 2 Welcome Tour (`040057C02159A000`) | Boots; runs very slowly |

### Known issues

- **Performance:** still a work in progress; expect slowdowns and stutter.
- **Donkey Kong Bananza:** destructible terrain is invisible on AMD GPUs.
- **Mario Kart World:** loading screens can take a long time, especially the first time a course is
  loaded. Recommended per-game settings: GPU accuracy Low, asynchronous shaders on, and ASTC
  recompression BC3 (needed on GPUs with less than 8 GB of video memory). Some geometry
  (task/mesh shaders) is missing.
- **Nintendo Switch 2 Welcome Tour:** reaches the intro and the tour, but runs very slowly.

## Building (Windows, MSYS2 MINGW64)

1. Install [MSYS2](https://www.msys2.org) and, in a **MINGW64** shell, install the MinGW toolchain,
   CMake, Ninja, Qt 6, FFmpeg, SDL3, Boost, OpenSSL and the Vulkan headers, plus git, patch and autotools
   (for example `pacman -S --needed mingw-w64-x86_64-{toolchain,cmake,ninja,qt6,ffmpeg,sdl3,boost,openssl,vulkan-headers} git patch autotools`;
   exact package names may differ — see [docs/Deps.md](docs/Deps.md)).
2. Configure and build from the repository root:

   ```sh
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DYUZU_TESTS=OFF \
         -DENABLE_WERROR=OFF -DYUZU_ROOM=OFF -DYUZU_CMD=OFF -DENABLE_LIBUSB=OFF \
         -Denet_FORCE_BUNDLED=ON -Dlibusb_FORCE_BUNDLED=ON -DVulkanMemoryAllocator_FORCE_BUNDLED=ON
   ninja -C build yuzu
   ```

3. Copy the MinGW runtime DLLs (Qt and its dependencies) next to `build/bin/eden.exe` and add a
   `build/bin/qt.conf` containing `[Paths]` / `Plugins = plugins` (see [docs/ns2/NOTES.md](docs/ns2/NOTES.md)).

The NS2 patch's MSVC script (`build-eden-msvc.bat`) is included but has not been tried with this tree.

## Building (Linux)

Not tested yet; nothing in the changes is Windows-only.

1. Install the dependencies for your distribution; [docs/Deps.md](docs/Deps.md) has full commands for
   Arch, Debian/Ubuntu/Mint, Fedora, Gentoo and others. For example:

   ```sh
   # Arch
   sudo pacman -Syu --needed base-devel boost catch2 cmake enet ffmpeg fmt git glslang libzip lz4 ninja \
        nlohmann-json openssl opus qt6-base qt6-multimedia qt6-charts sdl3 zlib zstd zip unzip \
        vulkan-headers vulkan-utility-libraries libusb spirv-tools spirv-headers
   ```

2. Configure and build from the repository root:

   ```sh
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DYUZU_TESTS=OFF \
         -DENABLE_WERROR=OFF
   ninja -C build yuzu
   ```

   If CMake cannot find a package your distribution doesn't ship (enet, libusb,
   VulkanMemoryAllocator, ...), add the matching `-D<name>_FORCE_BUNDLED=ON` flag from the Windows
   command above.
3. The binary is `build/bin/eden`; no DLL copying is needed.

## Running

Use your own dumped games, keys and firmware. Load a game from its extracted dump (the `romfs`
folder next to `exefs` is found automatically):

```sh
build/bin/eden -g "<dump>/updates/<version>/exefs/main"
```

Recommended settings: Vulkan, 12 GB memory layout, multi-core CPU.

## Licence

GPL-3.0-or-later, like Eden (see [LICENSE.txt](LICENSE.txt)). Not affiliated with the Eden project
or Nintendo; no game data, keys or firmware are included.
