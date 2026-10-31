> [!IMPORTANT]
> **AI disclaimer:** most of this fork's Android port (the code, docs and tooling on top of Eden-NS2) was written
> with the help of an AI coding assistant (Claude), then reviewed and tested by hand on a single phone. Treat it as
> experimental: bugs a careful human author would have caught can still be in it.

> [!WARNING]
> **Unofficial, experimental fork. It is not Eden, and the Eden team does not support it.**
> Please don't report problems with this build to Eden, Eden-NS2 or Takoroka.

# Eden SX2A

Switch 2 emulation on Android phones. Eden SX2A is the Android build of
[Eden-NS2](https://github.com/0x2593/Eden-NS2) (0x2593's Switch 2 fork of Eden): it follows Eden-NS2 with ordinary
merges and adds the Android app layer and the fixes phones need. Versions up to v0.2 were built on
[Takoroka's eden-nx2](https://git.eden-emu.dev/Takoroka/eden-nx2). Eden-NS2's own README follows this section.

Everything that was tested is built in and always on. There is nothing to configure beyond picking your games.

## Quick start

1. Download the APK from this repo's **Releases** page and check its SHA-256 against the release notes.
2. Install it and open **Eden SX2A**. On Adreno GPUs it selects the bundled Turnip driver by itself.
3. Open Settings → **Switch 2 readiness check** and fix anything it flags.
4. Add the folder that holds your loose Switch 2 dumps as a game folder, and grant **All files access** when asked.
5. Tap a game. The first load sits on a black screen for a few minutes while shaders are built.

## Compatibility

Tested on one phone: Honor foldable, Snapdragon SM8850 (Adreno 840), 16 GB RAM, bundled Turnip driver.

| Game | Version | Status |
| --- | --- | --- |
| Pokémon Pokopia (`0400A3A01EE4C000`) | update 327680 | **Playable**, about 20 FPS. First load about 4 minutes |
| Donkey Kong Bananza (`040074A01BF12000`) | update 458752 | **Playable** on Turnip, about 5-20 FPS, long loading |
| Mario Kart World (`0400C3F00006E000`) | 1.0.0 (no update) | **In game with graphical glitches**, about 6-10 FPS. Dark road, missing surfaces, flickering frames; may crash while a course loads |
| Nintendo Switch 2 Welcome Tour (`040057C02159A000`) | - | Boots; can crash after character creation |
| Other Switch 2 games | - | Untested on Android. Reports welcome |

| Device | Status |
| --- | --- |
| Adreno GPU, 16 GB RAM, bundled Turnip driver | Tested (the phone above) |
| Same phone, stock Qualcomm driver | Runs, but some draws fail (DK Bananza's ground is missing). Use Turnip |
| 12 GB RAM | May work for lighter games |
| Less than 12 GB RAM | Likely closed by Android while loading |
| Mali, PowerVR or other non-Adreno GPUs | Untested |

## Requirements

- arm64 phone with Android 13 or newer.
- **RAM:** 16 GB recommended; 12 GB may work for lighter games. DK Bananza peaks at about 6 GB for the app.
- **GPU:** Adreno. The app ships Mr. Purple's Turnip T30 (Mesa, MIT licence, 3.7 MB) and selects it on first start
  unless you already picked a driver. You can switch back to the system driver in the driver manager.
- **Games:** loose (extracted) Switch 2 dumps. NSP, XCI and other containers are not supported for Switch 2 titles.
  Keys and firmware are not needed for loose Switch 2 titles.

The app installs as `dev.eden.nx2`, alongside the official Eden app, with its own settings and saves. New versions
install over older Eden NX2 / Eden SX2A builds and keep your settings and saves.

## Adding games

A loose dump is a folder with `exefs/` (holding `main`, `main.npdm` and the other program files) and `romfs/`. The app
boots `exefs/main` from its real path, which is why it needs **All files access**.

**From the games list (recommended).** Add the folder that holds your dumps, for example `Switch2/`, as a game folder.
Each game shows up once, with its newest update if you have one, otherwise the base game. These layouts are found:

- `Switch2/Game/exefs/` + `Switch2/Game/romfs/`
- `Switch2/Game/base/` and `Switch2/Game/updates/<version>/`
- DLC in `Switch2/Game/dlc/<title id>/<version>/romfs`

If you add a folder higher up, such as the storage root, turn on deep scan for it so updates are found too (refreshing
the list gets slower).

**One game at a time.** In the games list, tap the folder button → **Launch ExeFS game folder** and pick the game's
folder (or its `exefs/` folder). If the app finds several versions, it asks which one to start; the newest is first.

Switch 2 games are recognised from their `main.npdm` and get the 12 GB memory layout automatically, only while they
run.

## Switch 2 readiness check

Settings → **Switch 2 readiness check** (the first entry) checks your setup and marks each item as pass, warning or
fail, with a button to fix it where the app has a screen for that:

- 64-bit phone and Android 13 or newer
- RAM (16 GB pass, 12 GB warning)
- Adreno GPU and the bundled Turnip driver
- recommended settings: Dynarmic CPU backend, asynchronous shaders on, async presentation off, resolution 1x or lower
- All files access, and at least one Switch 2 game found
- free storage (shader caches and logs grow) and battery usage set to Unrestricted

It runs again when you come back to it, or with **Run again**.

## Defaults

CPU backend **Dynarmic**, GPU mode **Fast**, resolution **1x**, asynchronous shaders **on**, async presentation
**off**. Switch 2 games run the GPU work on two threads with the faster command-list flush, which is quicker on
phones than one thread.

## Troubleshooting

Start with the readiness check: it finds most setup problems on its own.

**A game is missing from the games list.** Grant All files access, check that the folder you added contains the game
(or turn on deep scan for it), then refresh the list. **Launch ExeFS game folder** works either way.

**Black screen after launching.**
- Wait: loading takes a few minutes before the first frame.
- Keep the screen on and the app in front; emulation waits in the background.
- DK Bananza asks for accessibility settings on first boot: press A on the on-screen controller.
- Check that the Turnip driver is selected (Settings → GPU driver manager).
- If nothing happens for many minutes, check the dump: damaged program files (`main`, `sdk`, `subsdk0` that fail to
  decompress) hang without an error, and the log shows `assert r == int(nso_header.segments[i].size)` near the start.
- Still stuck? Share `Android/data/dev.eden.nx2/files/log/eden_log.txt` with your phone model, RAM and driver.

**The app closes on its own, with no error.** Usually Android's low-memory killer, not a crash. Keep the game in front,
close other apps, set battery usage for Eden SX2A to Unrestricted, and exclude it from any "app freezer" or "smart
memory" feature your phone has.

**Low frame rate.** Keep asynchronous shaders on, close other apps and let the phone cool down. Skip in-game videos
where you can: they play at the game's frame rate.

**Starting another game.** Swipe Eden SX2A away from recent apps first; that ends the game and the app.

## Known problems

- **Frame rate** is low: about 5-20 FPS in DK Bananza, around 20 FPS in Pokopia, 6-10 in Mario Kart World.
- **Mario Kart World 1.0.0:** its mesh shaders are not supported yet (dark road, missing surfaces), some frames flicker
  during races, and it can crash with a GPU error while a course loads. Newer updates are untested.
- **First loads** take minutes while shaders are built; later loads are faster.
- **Stock Qualcomm driver:** two pipelines fail to build, so parts of some scenes are missing.
- **Videos** may play slowly.
- **Memory:** about 4-5 GB (RAM plus swap) in DK Bananza.

## Be careful about

- **Legal:** only use games, keys and firmware dumped from hardware you own. None are included or provided.
- **Signing key:** the APK is signed with the public test key from Eden's source, so anyone could sign another
  "Eden SX2A" APK that installs over this one. Only install builds from this repo, and check the checksum.
- **All files access** lets the app read and write all of shared storage. It is only needed for loose dumps; you can
  revoke it when you're done.
- **Memory layouts:** the 10 GB and 12 GB layouts are unsafe; Android may close the app, or other apps, under memory
  pressure.
- **Heat and battery:** expect the phone to get hot and the battery to drain fast.
- **Saves:** back up saves you care about, under `Android/data/dev.eden.nx2/files/`.

## What this fork changes on top of Eden-NS2

- **Android app:** loose ExeFS dumps launched by real path and listed from game folders, the Switch 2 readiness check,
  loose DLC, 10 GB and 12 GB memory layouts, Switch 2 defaults, the bundled Turnip driver, and ending the game when the
  app is swiped away.
- **arm64 macro JIT:** the GPU's macro programs had only an x86-64 JIT; an arm64 backend (oaknut) runs them about 5x
  faster than the interpreter, fuzzed against it with 0 mismatches (`tools/nx2/jit_fuzz/`).
- **Adreno fixes:** the storage buffer limit on Qualcomm drivers, no host occlusion queries for Switch 2 games (they
  crashed the driver), the storage CAS helper the stock driver needs, skipping draws whose pipeline fails to build, and
  dropping methods sent to unbound subchannels.
- **GPU thread split on phones:** fewer waits between the two GPU threads (per-call-site drains, acquires and binds
  that skip the drain, a separate epoch for DMA reads), so the split is on by default on Android.
- **Pushbuffer flush:** Eden-NS2's flush of GPU-written command words (DK Bananza's destructible terrain) is on for
  Switch 2 games, skips pages already flushed since the last compute or copy work, and flushes the rest of a command
  list's GPU-written pages ahead with one host GPU wait.
- **Memory:** Dynarmic's arm64 per-block metadata in vectors (it was over 1 GiB) and guest thread stacks left
  uninitialized (about 490 MiB saved).
- **Game fixes:** a crash on unmapped texture buffers (Pokopia), and Mario Kart World 1.0.0's older SDK (graphics boot
  flags, GPU session and its address space partition queries, display interface version 1, output encoding 0x5).
- **Shaders and video:** more Switch 2 shader forms (a multi-channel bindless 2D sample for Welcome Tour; uniform shifts,
  reductions with uniform pointers and uniform predicate tables for Mario Kart World), Generic31Y/Z/W attributes, and
  faster video frame output.

Licence: GPLv3 or later, same as Eden. The bundled Turnip driver is Mesa (MIT licence), unmodified from its official
release. Every release is built from the tagged commit in this repo. More detail: [NX2_ANDROID.md](NX2_ANDROID.md).

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
