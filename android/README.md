# The Android app

The game wrapped in SDL's Java activity. What is here is the Gradle project
only; the native side is the repository's own `CMakeLists.txt`, which the
Android Gradle plugin runs with the NDK toolchain, and the libraries it
needs come from `tools/android/build-deps.sh`. The port's state, what has
been verified and what has not, is in `docs/android-port-2026-09-19.md`;
this file is the recipe.

## Prerequisites

- Android Studio, or the SDK command-line tools, with **SDK Platform 34**,
  **NDK 27.2.12479018** (r27c; `-PndkVersion=...` picks another 27) and
  **CMake 3.22.1** installed through the SDK manager.
- `cmake` (3.21 or later), `ninja`, `curl` and `tar` on the path, for the
  library build below.
- The game data (`Data/`), which is not in this repository.

## 1. Build the libraries

Android has no package manager the build can ask for SDL2, so the SDL
family and libjpeg-turbo are built from source, once per ABI:

```bash
export ANDROID_NDK_HOME=$HOME/Android/Sdk/ndk/27.2.12479018
tools/android/build-deps.sh arm64-v8a
```

About two minutes. It downloads the pinned release tarballs into
`build/android/src`, installs the libraries under
`build/android/deps/arm64-v8a`, copies SDL's Java classes to
`build/android/java` and the four shared libraries to
`build/android/jniLibs/arm64-v8a`, which is where `app/build.gradle`
looks for them. Another ABI is another run with its name as the
argument, plus that name in `abiFilters`.

## 2. Build the APK

```bash
cd android
gradle assembleDebug        # or open android/ in Android Studio
```

The plugin configures `../CMakeLists.txt` for the NDK with the same cache
the `android` preset uses (`BUILD_TESTS=OFF`, `BUILD_ENGINE=OFF`, API 28),
builds the `DarkEden` target - `libmain.so`, the game as a shared library -
and packages it with the SDL libraries, `BootstrapActivity` and
`DarkEdenActivity`. The APK is `app/build/outputs/apk/debug/app-debug.apk`,
about 18 MB: the game data is not in it.

To build only the native library, without the SDK, from the repository
root:

```bash
cmake --preset android
cmake --build --preset android      # build/presets/android/bin/libmain.so
```

`tools/ci/verify-android.sh` runs both steps the way
`.github/workflows/android.yml` does.

## 3. Install and first launch

```bash
adb install app/build/outputs/apk/debug/app-debug.apk
```

The data tree is not in the repository or the APK; it is a release of the
repository (`assets-v2`, one 862 MB zip holding `Data/` and an empty
`UserSet/`, 1.8 GB unpacked). The icon opens `BootstrapActivity`, which on
the first launch downloads that zip into the app's cache, checks it
against the SHA-256 pinned in `AssetInstaller.java`, unpacks it into the
app's internal files directory and writes a marker holding the release
tag, with a progress bar throughout and a retry button on a failure; the
download resumes from where it stopped. Every later launch finds the
marker and starts the game at once. Bumping the release is the four
constants at the top of `AssetInstaller.java`, and the next launch fetches
it. The device needs about 2.7 GB free during the install (the zip and
the tree), 1.8 GB after; the zip is deleted once unpacked. The same class
runs on a desktop JVM, which is how it was verified against the real
release without a device:

```bash
javac -d /tmp/ai app/src/main/java/org/opendarkeden/client/AssetInstaller.java
java -cp /tmp/ai org.opendarkeden.client.AssetInstaller /tmp/darkeden
```

The game then looks for `Data/Info/FileDef.inf` under the app's external
files directory first and its internal one second (`Client/Client.cpp`,
the data-root search), and runs from the first that has it, writing
`UserSet/`, `Log/` and the profile directory beside it. The external
directory comes first, and the bootstrap treats a tree there as
installed, so a tree pushed by hand overrides the download during
development:

```bash
adb push Data /sdcard/Android/data/org.opendarkeden.client/files/Data
```

Whatever the game would have printed to a terminal goes to the system
log under the tag `DarkEden` (`Client/SDLMain.cpp`, the logcat bridge),
so a start that shows nothing is read with:

```bash
adb logcat -s DarkEden SDL
```

The client's own `DarkEden.conf` lands in the app's internal files
directory (`SDL_GetPrefPath`), not beside the data.

## What to expect

A tap is a left click (SDL synthesises mouse events from touch), so
menus and click-to-move work in the crude sense; there is no hover, no
right-click and no keyboard beyond the on-screen one that text fields
raise. The UI is anchored to the 800x600 frame, scaled to the screen with
pillarboxes, so hit targets on a phone are small; a tablet is the
realistic device until the touch interface exists. The port document has
the list.
