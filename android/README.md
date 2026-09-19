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
and packages it with the SDL libraries and `DarkEdenActivity`. The APK is
`app/build/outputs/apk/debug/app-debug.apk`.

To build only the native library, without the SDK, from the repository
root:

```bash
cmake --preset android
cmake --build --preset android      # build/presets/android/bin/libmain.so
```

`tools/ci/verify-android.sh` runs both steps the way
`.github/workflows/android.yml` does.

## 3. Install the data

The game looks for `Data/Info/FileDef.inf` under the app's external files
directory first, then its internal one (`Client/Client.cpp`, the data-root
search), and runs from the first that has it; it writes `UserSet/`, `Log/`
and the profile directory beside the data, as on the desktop, so the data
must go somewhere writable. After the first install:

```bash
adb install app/build/outputs/apk/debug/app-debug.apk
adb push Data /sdcard/Android/data/org.opendarkeden.client/files/Data
```

The tree is large; the push takes a while. Whatever the game would have
printed to a terminal goes to the system log under the tag `DarkEden`
(`Client/SDLMain.cpp`, the logcat bridge), so a start that shows nothing
is read with:

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
