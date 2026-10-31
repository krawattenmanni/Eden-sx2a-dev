> [!IMPORTANT]
> **AI disclaimer:** most of this fork's Android port (the code, docs and tooling on top of Eden-NS2) was written
> with the help of an AI coding assistant (Claude), then checked and tested by hand on one phone. It is
> experimental and can have bugs.

> [!WARNING]
> **This is not the official Eden app, and the Eden team does not support it.**
> Please don't report problems with this app to Eden, Eden-NS2 or Takoroka.

# Eden SX2A

**Play Switch 2 games on your Android phone.**

This app is based on [Eden-NS2](https://github.com/0x2593/Eden-NS2). Older versions (up to v0.2) were based on
[Takoroka's eden-nx2](https://git.eden-emu.dev/Takoroka/eden-nx2). Eden-NS2's own README is further down.

You don't need to change any settings. Everything is already set up.

## What you need

- An Android phone with **Android 13 or newer**.
- **16 GB of RAM** is best. 12 GB may work for small games.
- A phone with an **Adreno** GPU (most Snapdragon phones). Other phones are not tested.
- Your own Switch 2 games, as **loose folders** (a folder with `exefs` and `romfs` inside). NSP and XCI files do
  not work for Switch 2 games.
- You do **not** need keys or firmware for Switch 2 games.

## How to start (5 steps)

1. **Download** the APK from this page: **Releases** → newest version.
2. **Install** it. The app is called **Eden SX2A**.
3. **Open the app.** It picks the right graphics driver for you.
4. **Add your games** (see below).
5. **Tap a game.** The first start shows a black screen for a few minutes. This is normal. Wait.

## How to add your games

You have two ways. Both find your games for you.

### Way 1: add your games folder (best)

The app looks inside the folder and finds every game by itself.

1. Put all your games in one folder, for example `Switch2`.
2. In the app, add that folder as a **game folder** (Settings → **Game folders** → **Add game folder**).
3. When the app asks for **All files access**, say **yes**. Games don't start without it.
4. Your games show up in the list. If a game has an update, the app shows the newest one.

**Deep scan:** if your games are far inside other folders (for example you picked the whole phone storage), turn on
**deep scan** for that folder. The app then looks deeper. It is slower, but it finds more.

### Way 2: pick one game

1. In the games list, tap the **folder button**.
2. Tap **Launch ExeFS game folder**.
3. Pick the folder of one game. The app looks inside and finds the game by itself.
4. If it finds more than one version, it asks which one you want. The newest is at the top.

### Folders the app understands

```
Switch2/
  Game Name/
    exefs/ and romfs/              <- a game
  Other Game/
    base/                          <- the game
    updates/123456/                <- an update (the app picks the newest)
    dlc/<title id>/<version>/romfs <- extra content
```

## Check your phone: "Switch 2 readiness check"

Open **Settings** → **Switch 2 readiness check** (the first item).

It checks your phone and shows what is missing:

- **Pass** = good
- **Warning** = may cause problems
- **Fail** = you must fix this

Most items have a **button** that takes you to the right place to fix it. Tap **Run again** after you fix something.

It checks: Android version, RAM, graphics chip and driver, settings, All files access, if games were found, free space,
and battery setting.

## Which games work

Tested on one phone (Snapdragon SM8850, 16 GB RAM).

| Game | Works? |
| --- | --- |
| Pokémon Pokopia (update 327680) | ✅ **Playable**, about 20 FPS. First start takes about 4 minutes |
| Donkey Kong Bananza (update 458752) | ✅ **Playable**, about 5-20 FPS. Slow loading |
| Mario Kart World (1.0.0, no update) | ⚠️ **In game with graphical glitches**, about 6-10 FPS. Dark road, missing parts, flickering. Can crash while a track loads |
| Nintendo Switch 2 Welcome Tour | ⚠️ Starts, can crash after you make your character |
| Other games | ❓ Not tested. Tell us how they run |

## If something goes wrong

**First, open the Switch 2 readiness check.** It finds most problems.

**My game is not in the list.**
- Did you allow **All files access**? You need it.
- Is the game inside the folder you added? If it is deep inside other folders, turn on **deep scan**.
- Pull down the list to refresh it.
- You can always use **Way 2** (pick one game).

**Black screen after I tap a game.**
- **Wait a few minutes.** The first start is slow.
- Keep the screen **on** and the app **open**. The game pauses in the background.
- Donkey Kong Bananza asks about accessibility the first time: press **A**.
- Black for a very long time? Your game files may be broken. Dump the game again.

**The app closes by itself.**
- Your phone ran out of memory. Close other apps first.
- Set the battery setting for Eden SX2A to **Unrestricted**.
- Turn off "app freezer" or "memory saver" for Eden SX2A, if your phone has it.

**The game is slow.**
- Close other apps. Let the phone cool down.
- Skip videos in the game if you can. Videos are slow.

**Switching to another game.** Close Eden SX2A first (swipe it away from recent apps), then open it again.

**Still stuck?** Share the file `Android/data/dev.eden.nx2/files/log/eden_log.txt`, your phone model, and how much RAM
it has.

## Known problems

- Games run slowly (low FPS).
- Mario Kart World has graphical glitches and can crash while a track loads.
- The first start of a game is slow. Later starts are faster.
- The phone's own graphics driver (not the bundled one) misses some graphics. Use the bundled driver.
- Videos in games can be slow.

## Please be careful

- **Only use games you own.** This app has no games, keys or firmware.
- **Only download the app from this page.** Other copies could be fake.
- **All files access** lets the app see all your files. You can turn it off when you are done playing.
- Your phone **gets hot** and the **battery drains fast**.
- **Back up your saves** (`Android/data/dev.eden.nx2/files/`). The app can crash.

## Updating

Install the new APK over the old one. Your settings and saves stay.

The app installs next to the official Eden app (it is a separate app: `dev.eden.nx2`).

## For developers

- **Android app:** loose ExeFS dumps launched by real path and listed from game folders, the Switch 2 readiness check,
  loose DLC, 10 GB and 12 GB memory layouts (Switch 2 games get 12 GB automatically, read from `main.npdm`), Switch 2
  defaults, the bundled Turnip driver (Mr. Purple's Turnip T30, Mesa, MIT licence), and ending the game when the app is
  swiped away.
- **arm64 macro JIT:** an arm64 backend (oaknut) for the GPU's macro programs, about 5x faster than the interpreter,
  fuzzed against it with 0 mismatches (`tools/nx2/jit_fuzz/`).
- **Adreno fixes:** the storage buffer limit on Qualcomm drivers, no host occlusion queries for Switch 2 games, the
  storage CAS helper the stock driver needs, skipping draws whose pipeline fails to build, and dropping methods sent to
  unbound subchannels.
- **GPU thread split on phones:** fewer waits between the two GPU threads, so the split is on by default on Android.
- **Pushbuffer flush:** Eden-NS2's flush of GPU-written command words is on for Switch 2 games, skips pages already
  flushed since the last compute or copy work, and flushes a command list's other GPU-written pages ahead with one wait.
- **Memory:** Dynarmic's arm64 per-block metadata in vectors (it was over 1 GiB) and guest thread stacks left
  uninitialized (about 490 MiB saved).
- **Game fixes:** unmapped texture buffers (Pokopia), and Mario Kart World 1.0.0's older SDK (graphics boot flags, GPU
  session and its address space partition queries, display interface version 1, output encoding 0x5).
- **Shaders and video:** more Switch 2 shader forms (multi-channel bindless 2D sample; uniform shifts, reductions with
  uniform pointers and uniform predicate tables), Generic31Y/Z/W attributes, and faster video frame output.
- All tested fixes are built in and always on in release APKs.

Licence: GPLv3 or later, same as Eden. The bundled Turnip driver is unmodified Mesa (MIT licence). Every release is
built from the tagged commit in this repo. The APK is signed with the public test key from Eden's source. More detail:
[NX2_ANDROID.md](NX2_ANDROID.md).

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
