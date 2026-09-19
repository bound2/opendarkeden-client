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

### The game data: downloaded on the first launch

The data tree is not in the repository; it is a release of it
(`assets-v2`: one 862 MB zip, `Data/` and an empty `UserSet/`, 1.8 GB
unpacked, about 2,020 files). It is not in the APK either - the first
cut put it there and the APK was 965 MB, which the repo owner did not
want - so the app fetches it:

- **`android/.../AssetInstaller.java`** downloads the zip into the app's
  cache with a resumable request (a download of that size over a
  phone's link stops sometimes; a Range request continues it), checks
  the complete file against the SHA-256 pinned in the class, unpacks it
  with `java.util.zip.ZipFile` into the app's internal files directory
  - the directory SDL reports as internal storage and the native
  data-root search looks under - and writes a marker holding the
  release tag last, so a later launch returns at once, an interrupted
  install is redone, and an upgrade of the pinned constants fetches the
  new release. Plain Java, nothing from `android.*`: the same class runs
  on a desktop JVM through its `main`, which is how it was verified
  against the real release (below). Two Windows leftovers in the release
  are skipped, a `.lnk` shortcut and a "copy" of `ServerInfo.inf`, both
  with non-ASCII names; the game's tables spell every path in ASCII, so
  nothing it opens is lost.
- **`BootstrapActivity.java`** is the launcher: it hands over to the SDL
  activity at once when the marker names the pinned release or a tree
  pushed by hand sits in external storage, and otherwise shows a
  progress bar over the installer with a retry button on failure.
  `SDLActivity` is not the place for this: it starts the native side in
  its `onCreate`, and the game would sit in a black window while the
  tree arrived. `DarkEdenActivity` is no longer exported and has no
  launcher entry.
- Nothing on the native side: the data-root search already looks under
  internal storage, and R18 is back at 8. A store listing would have to
  choose between this and Play Asset Delivery; sideloading does not
  care. The download runs over whatever network the phone has, with the
  size on screen and no metered-network prompt; that is a screen for
  later.

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
| `gradle assembleDebug` in `android/` | **`app-debug.apk`, 18 MB**: `libmain.so` (16 MB, Debug), the four SDL libraries, the SDL Java classes, `BootstrapActivity` and `DarkEdenActivity` in three dex files; no data |
| `AssetInstaller` on the desktop JVM against the real release | full run: 862 MB downloaded, SHA-256 matched, 2,018 files unpacked (1.8 GB), the two Windows leftovers skipped by name, marker written, zip deleted; a second run returns at once; a run started with the first 500 MB already in the cache resumed at 58% through GitHub's redirect with the Range header honoured and finished with the same tree |
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
   linkage), whether the data-root search then finds the downloaded
   tree, whether the fonts listed exist on the device at hand.
2. **The download on a device.** The installer is verified on a desktop
   JVM against the real release, resume included; on a phone the
   unknowns are the time it takes, what the OS does to a two-minute
   foreground download when the screen is locked (the activity keeps
   the screen on, and a killed download resumes on the next launch),
   and Android's own HTTP stack following GitHub's redirect with the
   Range header intact.
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
