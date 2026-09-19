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

The first build runs `tools/android/fetch-assets.sh` (the `fetchAssets`
task), which downloads the assets release - `darkeden-assets-v2.zip`,
862 MB, from https://github.com/bound2/opendarkeden-client/releases/tag/assets-v2 -
into `build/android/src`, checks it against the release's SHA-256,
unpacks `Data/` and `UserSet/` (1.8 GB) under `build/android/assets` and
writes the manifest beside them. Later builds see the manifest and skip
the fetch; delete `build/android/assets` to fetch again, or run the
script by hand with another release tag. On Windows run the script under
Git Bash before Gradle.

The plugin then configures `../CMakeLists.txt` for the NDK with the same
cache the `android` preset uses (`BUILD_TESTS=OFF`, `BUILD_ENGINE=OFF`,
API 28), builds the `DarkEden` target - `libmain.so`, the game as a shared
library - and packages it with the SDL libraries, `DarkEdenActivity` and
the data tree as the APK's assets. The APK is
`app/build/outputs/apk/debug/app-debug.apk`, about 965 MB (the tree deflates to a little more than the release zip).

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

The APK cannot be read in place - the game opens its files by path after
changing into the data root, and an APK's assets are zip entries - so the
first launch copies the tree out of the package into the app's internal
files directory, guided by the manifest (`basic/BundledAssets.h`; the
Android asset manager cannot list subdirectories, which is why the list
ships with the tree). It is 1.8 GB and takes a while on a phone; the
window is black meanwhile and logcat reports every hundredth file. A
marker file holding the release tag is written last, so a later launch
returns at once, an interrupted copy is redone, and an upgrade whose
manifest names another tag copies again. The device needs about three
times the tree's size free during the install: the APK, the copy, and
the package manager's own staging.

The game then looks for `Data/Info/FileDef.inf` under the app's external
files directory first and its internal one second (`Client/Client.cpp`,
the data-root search), and runs from the first that has it, writing
`UserSet/`, `Log/` and the profile directory beside it. The external
directory comes first so a tree pushed by hand overrides the bundled one
during development:

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
