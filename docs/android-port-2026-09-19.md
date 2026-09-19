# Android port

**Date:** 2026-09-19

**Scope:** the first slice of the Android port - the build, the packaging,
where a phone differs from a desktop in the startup path, and a Gradle
project that wraps the result - with the iOS preparation that fell out of
the same edits. What it does not do is make the game playable by touch;
that is the second half, sized at the end. This is the document the
assessment in the answer of 2026-09-19 asked for: an NDK compile probe
whose error list replaces the estimate.

**Branch:** `claude/game-android-ios-support-cnz3d4`.

## Executive conclusion

The game compiles and links for Android arm64 as `libmain.so`, the shared
library SDL's Java activity loads, from the same source files as Linux
and macOS, with **no source change beyond the platform-specific ones
listed below** - the port assessment's claim that the tree is SDL-only
held under the NDK. The Gradle project under `android/` wraps it in an
APK. What has not happened is a launch: no device or emulator was
available, so nothing past the link is verified, and the section
*Unverified* is the honest list.

## What changed

### Platform detection (`basic/Platform.h`)

Android defines `__linux__` and Apple's `TARGET_OS_MAC` is 1 on iOS too,
so before this slice both read as desktops. Now:

| Macro | Defined when | Also defines |
|---|---|---|
| `PLATFORM_ANDROID` | `__ANDROID__` | `PLATFORM_LINUX`, `PLATFORM_POSIX` |
| `PLATFORM_IOS` | `TARGET_OS_IPHONE` | `PLATFORM_POSIX` (not `PLATFORM_MACOS`) |
| `PLATFORM_MOBILE` | either of the above | |

Android keeps `PLATFORM_LINUX` because every branch under it holds
there (bionic has `/proc/self/exe`, `dirname`, two-argument `mkdir`);
iOS does not get `PLATFORM_MACOS` because the branches under that one -
`_NSGetExecutablePath`, the `/System/Library/Fonts` list, the bundle
data search - are the ones that do not hold. Ratchet **R18**
(`tests/ratchet/ratchets.sh`) holds the three spellings outside
`basic/` at their eight sites, the way R13 holds the desktop ones, so
"is this a phone" stays where a phone differs and does not spread into
game logic, where `PLATFORM_POSIX` is the right test.

### Where a phone differs in the startup path

Three places, all executable-side or in `basic`, all behind
`PLATFORM_MOBILE` or the specific platform:

- **The data root** (`Client/Client.cpp`, the off-Windows search). A
  desktop offers the working directory, the executable's directory and
  the directory beside a macOS bundle. A phone has none of those
  (working directory `/`, executable `/system/bin/app_process64`), so
  the candidates are replaced: on Android the app's external files
  directory (`/sdcard/Android/data/org.opendarkeden.client/files`,
  reachable by `adb push` and a file manager without root) and then
  its internal one; on iOS `SDL_GetPrefPath` and then the bundle's
  resources. Both are writable on Android, which matters because the
  game writes `UserSet/`, `Log/` and the profile directory beside its
  data.
- **The config file** (`basic/PlatformSDL.cpp`). Beside the executable
  on a desktop; `SDL_GetPrefPath("opendarkeden", "DarkEden")` on a
  phone, the per-app writable directory on both platforms.
- **Fonts** (`Client/TextSystem/TextBackendSDL.cpp`). Android's
  `/system/fonts`: Noto Sans CJK (the system CJK font since 5.0, split
  per region on some vendor images), DroidSansFallback for older
  images, Roboto as the Latin-only last resort. iOS keeps its fonts in
  an asset catalog SDL_ttf cannot open, so the data tree has to ship
  one there; the list says so and adds nothing.

### The app lifecycle (`Client/SDLMain.cpp`, `Client/CSDLGraphicsFlip.cpp`)

A mobile OS kills a process that draws while in the background, and
raises `SDL_APP_WILLENTERBACKGROUND` on its own thread the moment it
happens - the game's loop may not run again first. SDL's rule is to take
these in an event watch, not from the queue, and the entry point now
installs one: backgrounding sets `g_bPresentSuspended` (atomic, since
the watch runs on the Java UI thread on Android) and `Flip` composes but
does not present while it holds; foregrounding clears it;
`SDL_APP_TERMINATING` pushes the `SDL_QUIT` the pump already turns into
a clean shutdown. A desktop raises none of these; the watch is installed
everywhere and costs one switch per event. The orientation hint asks for
both landscapes before `SDL_Init`, because the 800x600 frame is
landscape and a portrait window would letterbox it to a third of the
screen.

### Logging (`Client/SDLMain.cpp`, Android only)

An Android app has no console: every `fprintf(stderr, ...)` in the
startup path, DebugLog's console echo and SDL's own complaints went
nowhere. The logcat bridge replaces both descriptors with a pipe and
drains it on a thread into the system log under the tag `DarkEden`, so a
start that shows nothing is `adb logcat -s DarkEden SDL` away instead of
invisible.

### Bundled assets

The data tree is not in the repository; it is a release of it
(`assets-v2`: one 862 MB zip, `Data/` and an empty `UserSet/`, 1.8 GB
unpacked, 2,020 files in 10 directories). The APK carries it, and the
first launch copies it out:

- **`tools/android/fetch-assets.sh`** downloads the zip, checks it
  against the release's SHA-256, unpacks it under
  `build/android/assets` and writes `darkeden-assets.manifest` beside
  it: a header, `version <tag>`, `dir <path>` per directory,
  `file <size> <path>` per file, size first because the tree has names
  with spaces, sorted so the same tree gives the same manifest. The
  Gradle project runs it when the manifest is missing and packages the
  directory as the APK's assets.
- **`basic/BundledAssets.cpp`** does the copy on the device. An APK's
  assets are zip entries readable only through the asset manager, and
  the game opens its files by path after changing into the data root,
  so the tree has to be on the file system; the asset manager can list
  a directory's files but not its subdirectories, so the native side
  needs the manifest. Every source is read through `SDL_RWFromFile`,
  which on Android reads an asset for a relative path and a file for a
  directory-rooted one elsewhere, so the unit test
  (`tests/unit/test_bundled_assets.cpp`, 8 tests) runs the same code
  over a scratch directory: the copy, the marker written last, the
  no-op second launch, the version bump that overwrites, the size
  mismatch that fails without a marker, the interrupted copy that is
  redone because the old marker goes first, and the malformed and
  path-escaping manifests that are rejected before anything is written.
  In `basic` for that reason: the caller is the executable's entry
  point, and only a library gets a test.
- **`Client/SDLMain.cpp`** calls it on Android after `SDL_Init`, into
  `SDL_AndroidGetInternalStoragePath()`, reporting progress and the
  outcome to logcat; a failure is not fatal, the data-root search goes
  on to external storage, where a tree pushed by hand may be, and the
  external directory is searched first so that tree overrides the
  bundled one during development. The copy takes a while on a phone and
  the window is black meanwhile; a progress screen is touch-interface
  work.
- What it costs: the APK is about 965 MB (2,018 files, the release zip deflated once more), and the device
  holds the package and the copy both, so about three times the tree
  during the install. Play Store distribution would need Play Asset
  Delivery instead of a fat APK; sideloading does not care.

### The build

- **`tools/android/build-deps.sh`** builds SDL2 (2.30.11), SDL2_image
  (2.8.4), SDL2_ttf (2.22.0, vendored FreeType), SDL2_mixer (2.8.1,
  minimp3 and stb_vorbis built in) and libjpeg-turbo (3.0.4, at the
  libjpeg 6.2 ABI `Client/JpegLib/jpeglib.h` declares) for one ABI with
  the NDK toolchain into `build/android/deps/<abi>`. There is no package
  manager to ask; vcpkg's `arm64-android` triplet would be the
  alternative and was not tried. SDL2_image is built with its vendored
  flag on although the stb backend vendors nothing, because its
  exported config otherwise demands libpng and libjpeg from every
  consumer - the one dependency surprise of the probe.
- **`CMakeLists.txt`**: on Android the prefix joins
  `CMAKE_FIND_ROOT_PATH` (the NDK toolchain confines every find to that
  list), so every `find_package(SDL2)` site in the tree resolves
  unchanged; `DarkEden` is `add_library(SHARED)` named `main` instead
  of an executable, linked with `--no-undefined` by the toolchain so
  the link check is as complete as an executable's; the resource and
  viewer tools and the engine sprite tool are skipped, being desktop
  programs.
- **`CMakePresets.json`**: the `android` configure and build presets
  (arm64-v8a, API 28 - bionic's iconv arrived in 28 - tests and engine
  off, toolchain from `ANDROID_NDK_HOME`).
- **`android/`**: the Gradle project. `app/build.gradle` runs the
  repository's CMake through the plugin with the same cache, takes SDL's
  Java classes and the four shared libraries from where the script put
  them, and packages `DarkEdenActivity`, an `SDLActivity` that names its
  libraries. `android/README.md` is the recipe.
- **`tools/ci/verify-android.sh`** and **`.github/workflows/android.yml`**:
  the libraries (built when missing), the preset's configure and build,
  and a check that `bin/libmain.so` exists, on every push and pull
  request, so the port cannot rot the way the macOS path did.

## Measured state

All measured on 2026-09-19 in a Linux container (Ubuntu 24.04, x86-64,
4 cores), NDK r27c, SDL 2.30.11, against this branch.

| Step | Result |
|---|---|
| `tools/android/build-deps.sh arm64-v8a` | five libraries built and installed, about two minutes |
| `cmake --preset android` | configures; every `find_package` resolves against the prefix |
| `cmake --build --preset android` | **1,162 translation units, 1 error, 41 warnings**; `bin/libmain.so` links |
| `gradle assembleDebug` in `android/` | **`app-debug.apk`**: `libmain.so` (16 MB, Debug), the four SDL libraries, the SDL Java classes and `DarkEdenActivity` in three dex files - 18 MB before the assets, **965 MB** with the 2,018-file data tree under `assets/` and its manifest |
| `tools/android/fetch-assets.sh` | 862 MB zip verified against the release's SHA-256, 1.8 GB unpacked, two Windows leftovers dropped (a `.lnk` and a "copy" of ServerInfo.inf, both with non-ASCII names Gradle could not hash under a non-UTF-8 locale) |
| `unit_tests` after the extractor | 828 tests, 657,069 checks, 0 failed (8 new); ratchets R13 = 1, R18 = 9 |
| `tools/ci/verify-linux.sh linux` after the changes | build, 9 ctest entries green; `unit_tests` 820 tests, 657,003 checks, 0 failed; ratchets R13 = 1 and R18 = 8 |

The one error was the whole port's source delta beyond the mobile
branches: `Client/ProfileManager.cpp` included `<sys/dir.h>`, the BSD
spelling of `<dirent.h>` that glibc and Darwin keep and bionic does not,
and used nothing from it. Removed. Everything else - the wire layer, the
sprite code, the UI framework, the game logic - compiled for arm64 Clang
19 as it stands, with the same warning classes GCC and Clang report on
Linux (`-Wcomment`, `-Wmacro-redefined`, `-Wswitch`, `-Wnull-arithmetic`).

The library exports `SDL_main` (checked with `llvm-readelf`: the one
symbol `SDLActivity` looks up by name) and needs `libSDL2.so`, the three
satellites, `liblog`, `libandroid`, `libm`, `libdl` and `libc`, all of
which the APK or the system carries.

The Gradle run used SDK Platform 34, build-tools 34.0.0, the SDK
manager's CMake 3.22.1, Android Gradle plugin 8.7.3 on Gradle 8.14.3 and
JDK 21, with the NDK linked in as `ndk/27.2.12479018`. The Android
workflow builds only the native library; the APK needs the SDK, which
the runner image also has, and wiring `assembleDebug` into the job is
the natural next step once someone has installed one on a device.

## Unverified

Nothing here has run on a device or an emulator; this container has
neither and cannot host one. In order of how soon each would bite:

1. **Startup.** Whether `SDLActivity` finds `SDL_main` in `libmain.so`
   (it should: `<SDL_main.h>` renames `main` and declares it with C
   linkage), whether the asset copy completes and the data-root search
   then finds the tree, whether the fonts listed exist on the device at
   hand.
2. **Installing the APK.** The build is verified; `adb install` of a
   package this size is not, nor the copy's duration on a real device,
   nor whether the asset manager streams a compressed 10 MB sprite pack
   without complaint (it should: `AAsset_read` inflates as it goes).
3. **Login and gameplay.** Unverified off Windows on any platform (the
   port assessment's area F, still open); a phone inherits that, and a
   failure there is far easier to chase on Linux first.
4. **Suspend and resume.** Android holds a paused app inside
   `SDL_PollEvent` (`SDL_HINT_ANDROID_BLOCK_ON_PAUSE`, on by default),
   so the game stops entirely while backgrounded and the server's
   timeout decides what it comes back to. Nothing reconnects.
5. **The GL context.** SDL recreates it on resume; the client uploads
   its one frame every present and holds no persistent textures beyond
   the scaler's, which `spritectl_present_surface` re-creates on a size
   change, so there should be nothing to lose. Not seen.
6. **Performance.** An 800x600 frame composed on the CPU is nothing for
   a phone; the xBRZ upscaler's factor is chosen from the pixel size and
   a 2400x1080 screen asks for 3x on the CPU every frame. It may need
   to default off on mobile.
7. **iOS.** Every iOS branch above has been written and never compiled;
   nobody here has a Mac, and an iOS build needs Xcode, a bundle, signing
   and an SDL built for iOS, none of which is here. What it gets from
   this slice is the detection, the paths, the lifecycle watch and the
   font note.

## What remains: the touch interface

This is the open-ended half, and none of it is an `#ifdef`. SDL
synthesises a left click from a tap, so click-to-move and menus work in
the crude sense from day one. Everything else does not:

- **Hover.** Tooltips and targeting read the mouse position between
  clicks; a finger has no position between taps.
- **Right-click.** Five sites in the client; a long-press mapping in the
  input pump is the obvious answer.
- **The keyboard.** 244 virtual-key references: chat, hotkeys, Escape
  menus, Tab, arrows. Text fields raise the on-screen keyboard through
  `SDL_StartTextInput`, which the client already calls; nothing else
  has a touch equivalent.
- **Hit targets.** The UI is anchored to the 800x600 frame and
  letterboxed onto the screen; on a 6-inch phone a 16-pixel button is
  under three millimetres. A larger UI skin, or a tablet.
- **Chat and the Korean IME.** Mobile keyboards compose Hangul
  themselves and deliver it as `SDL_TEXTINPUT`, which the edit widget
  already takes; `Ci_macOS.cpp` is not needed there but is harmless.

Sizing, one engineer: a tablet reaching the title screen and login is
days now, not weeks, once a device is at hand; the touch interface good
enough to play is one to three months and a design, in `VS_UI` (a
testable library) and the executable's input pump.
