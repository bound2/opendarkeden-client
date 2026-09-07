# opendarkeden-client

## Build on Windows (from scratch)

The current build is **CMake + SDL2 + Visual Studio 2022**. The old DirectX code
has been migrated to SDL2, so no DirectX SDK is needed any more.

### 1. Prerequisites

Install **Visual Studio 2022** with the *Desktop development with C++* workload.
That gives you the MSVC toolset (v143) and a Windows 10/11 SDK.

You also need the **C++ ATL** individual component, which that workload does
*not* install by default. `VS_UI/src/header/VS_UI_WebBrowser.h` includes
`atlbase.h` for the legacy in-game IE browser control, and those sources are
only excluded on non-Windows builds — so on Windows ATL is mandatory. In the
Visual Studio Installer: *Modify → Individual components → "C++ ATL for latest
v143 build tools (x86 & x64)"*. Or from an **elevated** terminal (`--quiet`
silently refuses with exit code 5007 if the shell isn't already elevated — it
does not prompt for UAC):

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "C:\Program Files\Microsoft Visual Studio\2022\Community" --add Microsoft.VisualStudio.Component.VC.ATL --quiet --norestart
```

Close Visual Studio first. The installer aborts with exit code 8006
(`Pre-check verification failed with warning(s) : VSProcessesRunning`) if
anything VS-related is alive, and "anything" is broader than it sounds. Beyond
`devenv` itself:

- idle `MSBuild.exe` worker nodes, which linger for ~15 minutes after a
  `cmake --build ... -- -m` run,
- `vctip.exe`, the VC++ telemetry uploader that `cl.exe` spawns and leaves
  behind,
- `cl.exe` and `Tracker.exe` orphaned by an interrupted build, which have no
  parent left to reap them and can sit there indefinitely.

Chasing those by name is a losing game — `taskkill /F /IM MSBuild.exe` does not
touch the last two. Ask instead what is running out of the Visual Studio
directory, which is the same question the installer is asking:

```powershell
Get-Process | Where-Object { $_.Path -like 'C:\Program Files*\Microsoft Visual Studio\*' } | Select-Object Id, ProcessName, Path
```

Everything it lists has to be gone before the installer will proceed. Note that
`--quiet` prints nothing on success, so check `$LASTEXITCODE`: `0` is done,
`3010` is done-but-wants-a-reboot, `8006` is the pre-check above.

Verify it landed before building — this should print an install path, not
nothing:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -products * -requires Microsoft.VisualStudio.Component.VC.ATL -property installationPath
```

Install CMake 3.21+ (the first version with the VS2022 generator) and reopen
your terminal so it lands on `PATH`:

```bash
winget install Kitware.CMake
```

### 2. Install the dependencies with vcpkg

`vcpkg.json` declares the dependencies and pins their baseline. Clone vcpkg,
then select that same revision before bootstrapping (run from this repo):

```bash
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
```

```powershell
$baseline = (Get-Content vcpkg.json -Raw | ConvertFrom-Json).'builtin-baseline'
git -C C:\vcpkg checkout $baseline
C:\vcpkg\bootstrap-vcpkg.bat
```

CMake installs these dependencies automatically when configuring a fresh build.
The first install can compile libraries from source and take a while. Existing
classic-mode build trees keep their old dependency setup; use the presets below
to validate the pinned manifest without changing those trees.

| Package | Used for |
| --- | --- |
| `sdl2`, `sdl2-image`, `sdl2-ttf` | rendering, sprites, text (required) |
| `sdl2-mixer` | sound and music |
| `libiconv` | encoding conversion in `MString.cpp` / `TextService.cpp` |
| `libjpeg-turbo` | JPG load/save in `UtilityFunction.cpp` |

vcpkg can live anywhere — just point `-DCMAKE_TOOLCHAIN_FILE` at wherever you
put it in the next step. `vcpkg integrate install` is optional.

### 3. Generate the solution

The toolchain file must be passed on the **first** configure, or vcpkg's
packages won't be found:

```bash
cmake -S . -B build/vs2022 -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 4. Build

```bash
cmake --build build/vs2022 --config Debug
```

Or open `build/vs2022/DarkEdenClient.sln` in Visual Studio, set **DarkEden** as
the startup project, and build from there.

The executable lands in `build/vs2022/bin/Debug/DarkEden.exe`.

### Reproducible builds and the complete test suite

The checked-in presets use separate trees under `build/presets/`, enable all
tests, and require Bash and Perl from Git for Windows. Add Git's `usr/bin`
directory to your terminal's `PATH`, or use the verification script below,
which locates those tools explicitly. Set `VCPKG_ROOT` for this terminal:

```powershell
$env:VCPKG_ROOT = 'C:/vcpkg'
cmake --preset windows
cmake --build --preset windows-debug
ctest --preset windows-debug
```

The same configured tree supports `windows-release` build and test presets.
For ASan, configure `windows-asan`, then use the `windows-asan` build and test
presets (the component described in step 5 is required). Configure the two
trees sequentially: they share a manifest dependency installation under
`build/presets/vcpkg_installed`.

A second compiler checks that the sources are ISO C++20 rather than MSVC's
dialect of it. The `windows-clang` preset builds every target with the
clang-cl that ships with Visual Studio's "C++ Clang tools for Windows"
component, through NMake, so it has to run from a **VS developer prompt**
(`vcvars64.bat`, or the "x64 Native Tools Command Prompt"), where `clang-cl`
and `nmake` are on the path:

```powershell
$env:VCPKG_ROOT = 'C:/vcpkg'
cmake --preset windows-clang
cmake --build --preset windows-clang
ctest --test-dir build/presets/windows-clang --output-on-failure
```

NMake compiles one file at a time, so expect this tree to take several times
longer than the Visual Studio one. The ratchet that counts translation units
in the executable (R1) is skipped here, because it reads a `.vcxproj` that
this generator does not produce.

To run the same complete verification as CI, including checking that every
required CTest entry is registered:

```powershell
./tools/ci/verify-windows.ps1 -ConfigurePreset windows -BuildPresets windows-debug,windows-release
./tools/ci/verify-windows.ps1 -ConfigurePreset windows-asan -BuildPresets windows-asan
```

If Windows PowerShell reports that scripts are disabled, run the script in a
child process with a policy override limited to that process:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& ./tools/ci/verify-windows.ps1 -ConfigurePreset windows -BuildPresets windows-debug,windows-release"
```

This builds every configured target, then runs the unit suite, ratchets,
include-graph rules, format-arity audit, packet-index audit, and wire-inventory
freshness check. Logs go under `build/verification/`. The Windows Actions
workflow runs these commands on pushes to `master` (including merged PRs) or
manual dispatch, caches dependency binaries, and retains diagnostic artifacts
for seven days. PR updates do not trigger CI. It does not require
game assets or launch the game; live-server smoke testing remains separate.

Use `CMakeUserPresets.json` for local overrides; it is ignored by git. When
updating dependencies, change the manifest baseline deliberately and check
the deployed DLL names against the client's startup whitelist.

### 5. AddressSanitizer (optional)

MSVC has its own AddressSanitizer, wired up behind `USE_ASAN`. It reports heap
and stack overflows, use-after-free and double frees at the instruction that
causes them, which is the only realistic way to reach the memory-safety defects
in code no unit test can link against.

It needs the **C++ AddressSanitizer** individual component, which the *Desktop
development with C++* workload does not install. Same procedure as ATL in step 1
— close Visual Studio first, and run from an **elevated** terminal:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\setup.exe" modify --installPath "C:\Program Files\Microsoft Visual Studio\2022\Community" --add Microsoft.VisualStudio.Component.VC.ASAN --quiet --norestart
```

Verify it landed:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -products * -requires Microsoft.VisualStudio.Component.VC.ASAN -property installationPath
```

Then generate a **separate** build tree. Do not turn `USE_ASAN` on in your normal
one: it changes the compile and link flags of every target, so the two cannot
share a cache.

```bash
cmake -S . -B build/vs2022-asan -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DUSE_ASAN=ON
cmake --build build/vs2022-asan --config Debug
```

Configure stops immediately with instructions if the component is missing,
rather than letting the build run and fail at link time.

Three things the build does on your behalf, because the sanitizer will not work
otherwise:

- **Drops `/RTC1`.** MSVC rejects the runtime checks outright next to
  `/fsanitize=address`. That is a real loss, since `/RTC1` is what catches
  uninitialised locals, so keep the ordinary Debug tree for day-to-day work and
  switch to this one when chasing a memory error.
- **Turns off incremental linking**, which the sanitizer does not support.
- **Copies `clang_rt.asan_dynamic-x86_64.dll` beside the executables.** Visual
  Studio puts the compiler directory on `PATH` when it launches the debugger, so
  F5 would find it regardless; running the `.exe` from a shell or from Explorer
  would not.

Expect roughly 2x the run time and appreciably more memory. A report goes to
stderr and the debugger breaks at the faulting instruction. Note that leak
detection is a Linux-only part of AddressSanitizer — this catches invalid
accesses, not leaks. The CRT leak dump at exit already covers those.

### Which Debug build to use

There are two, and neither subsumes the other. `/RTC1` and `/fsanitize=address`
are mutually exclusive, so the checks are split across them:

| | `build/vs2022` | `build/vs2022-asan` |
|---|---|---|
| Uninitialised local read | **`/RTC1`** | |
| Stack frame damage around a call | **`/RTC1`** | |
| Heap overflow, use-after-free, double free | | **ASan** |
| Stack buffer overflow | | **ASan** |
| Invalidated iterator, bad comparator | debug iterators | debug iterators |
| Speed | normal | ~2x slower |

Use the ordinary tree day to day. Switch to the sanitized one when you have a
memory error to chase — a wild pointer, a crash inside the allocator, damage
that shows up far from its cause.

`/RTC1` is requested explicitly by `CMakeLists.txt` rather than inherited from
CMake's built-in Debug flags, so overriding `CMAKE_CXX_FLAGS_DEBUG` cannot drop
it silently. Configure prints which checks are on:

```
-- Debug runtime checks: /RTC1 (uninitialised locals, stack frames)
-- AddressSanitizer enabled (/fsanitize=address)
```

Exactly one of those two lines should appear on a Debug configure. If neither
does, something has gone wrong with the flags.

### Never commit `build/`

CMake bakes **absolute paths** into the generated `.sln`/`.vcxproj` files, along
with the exact Windows SDK version and toolset of the machine that generated
them. Those files are not portable between clones, which is why `build/` is
gitignored. Always regenerate locally.

### Troubleshooting

**`The Windows SDK version 10.0.xxxxx.0 was not found`** or
**`The referenced project <some other path>\ZERO_CHECK.vcxproj does not exist`**

You're opening a solution that was generated on a different machine. "Retarget
solution" won't fix it — it only rewrites the SDK and toolset, leaving the
foreign absolute paths behind. Delete the stale build tree and regenerate with
step 3:

```powershell
Remove-Item -Recurse -Force build\vs2022
```

**`find_package(SDL2 REQUIRED)` fails / configure stops at `CMakeLists.txt:90`**

Either vcpkg packages are missing (step 2), or the toolchain file wasn't passed
on the first configure. Delete `build/vs2022` and redo step 3 — adding the
toolchain to an existing cache does not work.

**`fatal error C1083: Cannot open include file: 'ATLBASE.H'`**

The C++ ATL component is missing — see step 1. CMake locates `atlmfc/include` by
globbing installed VS instances at **configure** time, so after installing ATL
you must re-run step 3 (an incremental build alone won't pick it up).

**Link errors mentioning `iconv.lib`**

`libiconv:x64-windows` isn't installed. See step 2.

**`LNK1104: cannot open file 'atls.lib'`**

Same cause as the `ATLBASE.H` error above — ATL headers embed a
`#pragma comment(lib, "atls.lib")`. Install the ATL component and re-configure.

**`LNK1104: cannot open file 'clang_rt.asan_dynamic_runtime_thunk-x86_64.lib'`**

The C++ AddressSanitizer component is missing — see step 5. `cl.exe` accepts
`/fsanitize=address` without it and only fails at link time, which is why a
`USE_ASAN=ON` configure now checks for the runtime and stops early instead.

**The sanitized build exits immediately with a missing-DLL dialog**

`clang_rt.asan_dynamic-x86_64.dll` is not beside the executable. Re-run the
configure step: the copy happens at configure time, so a build tree generated
before the component was installed will not have it.


## Run

### Code and data are separate

This repository contains the client's **source code only** — no game assets.
The art, maps, audio and configuration are a separate ~1.8 GB download from the
original publisher, too large for git and not ours to redistribute. Building the
client gives you a complete, fully functional executable; it just needs that
data at runtime, the same way an open Doom or Quake engine still needs the
original WAD or PAK files.

Download it from [Mediafire](https://www.mediafire.com/file/017bif66kyieviw/DARKEDEN.zip/file)
or [Baidu Netdisk](https://pan.baidu.com/s/1-DufSEmnydMbOtTwOo_h8A) (extract
code `6bcl`) and unpack it anywhere you like.

The archive also contains `fengshen.exe` along with `DEUtil.dll`, `GL.dll`,
`IFC22.dll` and `basics.dll`. Those are the **old VC6 client** from ~2010, built
against DirectX. The CMake build replaces all of them: `DarkEden.exe` is the
same client built from this source against SDL2, and it links those libraries
statically from `basic/`, `Client/DXLib`, `Client/SpriteLib` and
`Client/framelib`. You do not need to copy any of them — only `Data/` and
`UserSet/`.

### Toggle xBRZ rendering

Open **Options > Graphics** and use **xBRZ smoothing**, immediately below the FPS
option. It is enabled by default, including with older settings files. Changes
apply immediately and are saved with the other options when you close the menu.
Uncheck it to restore original presentation. No restart or environment variable
is needed. Fullscreen and resolution are also available in Graphics; those two
settings take effect after restarting the client.

This filters the completed frame, including the world, effects, and UI. It uses
the existing assets and preserves game coordinates, mouse mapping, aspect ratio
and letterboxing. It does not add widescreen world visibility or replace artwork.
Because filtering happens after composition, character edges may differ from
the individual transparent sprites in the offline comparison gallery.

xBRZ runs on up to four CPU threads at an integer scale of 2x to 4x, then SDL fits the result to
the window. Buffers and textures are reused, and unchanged frames reuse the
filtered texture. The intermediate image is capped at 64 MiB; texture/allocation
failures restore original presentation. Moving scenes still require filtering
each new frame, so compare frame time with the toggle on your machine. The
vendored filter is compiled with `/O2` in every configuration, Debug included:
unoptimised it costs about three times as much per frame.

The pinned xBRZ source and upstream license are under `third_party/xbrz/`.

### Point the build at the data

`DarkEden.exe` resolves game files relative to its working directory, which is
its own folder (`CMakeLists.txt` sets `VS_DEBUGGER_WORKING_DIRECTORY` so that
F5 in Visual Studio matches double-clicking from Explorer). So `Data/` and
`UserSet/` must sit next to the executable.

Rather than copying 1.8 GB into the build tree, link it. From an ordinary
prompt — directory junctions do not need administrator rights:

```powershell
$dest = "build\vs2022\bin\Debug"
$src  = "C:\path\to\your\unpacked\darkeden"
New-Item -ItemType Junction -Path "$dest\Data"    -Target "$src\Data"
New-Item -ItemType Junction -Path "$dest\UserSet" -Target "$src\UserSet"
```

Repeat for `bin\Release` if you build that configuration.

### Launch

Double-click `DarkEden.exe`, or run it without arguments:

```powershell
.\build\vs2022\bin\Debug\DarkEden.exe
```

The default is **borderless fullscreen**, with the original 800x600 game view
scaled to your desktop and xBRZ smoothing enabled. In **Options > Graphics**:

- **Borderless fullscreen (restart)**: uncheck for a window.
- **1024x768 resolution (restart)**: check for the larger game view and UI layout;
  uncheck for the original 800x600 view.
- **xBRZ smoothing**: switches instantly.

Close Options to save. Display preferences live in `UserSet/Display.ini` and are
read before creating the window. On Windows, paths resolve from the EXE folder
even when a shortcut has a different working directory. Restart after changing
fullscreen or resolution; no launch arguments are needed.

Existing launcher arguments remain optional overrides for compatibility:

| Argument | Mode |
| --- | --- |
| `0000000001` | window, scaled |
| `0000000002` | fullscreen, scaled |
| `0000000003` | window, 1024x768 |
| `0000000004` | fullscreen, 1024x768 |

### Connect to a server

Set the login server IP and port in `Data/Info/GameClient.inf`. With a server in
Docker on the same machine, use `127.0.0.1`.
