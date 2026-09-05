# C++17/C++20 compatibility assessment

**Date:** 2026-09-04

**Scope:** Language-standard upgrade of the existing client, libraries, tests, and
tools. This is not an operating-system port or a general modernization rewrite.

**Current live platform:** Windows x64, Visual Studio 2022, vcpkg, SDL2.

## Executive conclusion

The project is close to **building in a newer MSVC language mode**, but it is not
currently ISO C++17- or C++20-conforming.

For the supported Windows/MSVC path, this is moderate work rather than a rewrite:

| Delivery level | Estimated effort for one engineer | Result |
|---|---:|---|
| C++17, current MSVC extensions allowed | **3-5 working days** | All targets build and tests pass in `/std:c++17`, but removed syntax remains and another conforming compiler will reject it. |
| C++20, current MSVC extensions allowed | **5-8 working days** | All targets build and tests pass in `/std:c++20`; the known `char*`/literal failures are fixed, but removed exception syntax and `register` remain as MSVC extensions. |
| ISO-clean C++20 with MSVC and Clang validation | **15-25 working days** | Removed syntax is eliminated, strict compilation is enabled, and both compiler paths are kept green. |

The range includes implementation, review, full builds, automated tests, and a
basic in-game smoke test. It does not include fixing unrelated warnings or doing a
Linux/macOS port.

**Recommendation:** target C++20 directly. The measured increment from C++17 to
C++20 is small compared with the shared conformance cleanup, and Visual Studio
2022 already supports the target. Do not first land a large C++17 cleanup and then
repeat the stabilization cycle for C++20.

If the requirement only means "the current Windows build accepts `/std:c++20`,"
budget about one engineer-week. If it means "valid C++20 that is checked by more
than MSVC," budget three to five engineer-weeks.

## What "compatible" means in this assessment

There are three materially different outcomes:

1. **MSVC build-compatible:** every configured target compiles and links with the
   selected `/std:c++17` or `/std:c++20` mode using Visual Studio's default
   permissiveness.
2. **ISO source-compatible:** project code does not depend on MSVC accepting
   constructs removed from the selected C++ standard. A current Clang build is the
   practical second check.
3. **Modernized:** old ownership, strings, containers, and error handling are
   redesigned to use newer standard-library facilities.

Only the first two are a port. The third is a much larger refactor and is not
included in the estimates.

Likewise, C++20 source compatibility does not make the non-Windows path supported.
The repository describes Windows/MSVC as the live path, has platform-specific ATL
and Win32 integration, and does not currently have a Linux/macOS CI build. An OS
port needs a separate assessment.

## Measured project size

The repository is large, which makes validation and large mechanical diffs more
expensive even though the number of root causes is small.

| Surface | Files | Lines |
|---|---:|---:|
| `.cpp` | 1,232 | 408,556 |
| `.c` | 45 | 14,947 |
| `.h` | 1,056 | 126,478 |
| Unique C/C++ implementation files in the generated target graph | 1,220 | 390,088 |

The largest configured projects are `packetwire` (525 translation units),
`DarkEden` (489), `VS_UI` (53), `unit_tests` (39), `gamemodel` (34), and
`SpriteLib` (30). This count includes every normal executable and tool, not only
the game executable.

The external libraries are not a major standard-version risk. SDL2, SDL2_image,
SDL2_ttf, SDL2_mixer, iconv, and libjpeg-turbo are consumed through stable C APIs
or supported CMake packages. The 14 configured `.c` files are compiled as C and
are unaffected by the selected C++ standard.

## Current build contract

The root build is explicitly pinned to C++11 at `CMakeLists.txt:13-14`, while both
the root and test projects claim a CMake 3.10 minimum. The existing MSVC projects
generated from that configuration contain no `LanguageStandard` setting because
MSVC has no selectable C++11 mode; the effective Windows mode is the compiler's
default C++14-like mode.

For a real upgrade, the build contract should:

- raise `CMAKE_CXX_STANDARD` directly to 20 (or 17 if that is the chosen target);
- keep `CMAKE_CXX_STANDARD_REQUIRED ON`;
- set `CMAKE_CXX_EXTENSIONS OFF`;
- raise the declared CMake minimum to the already documented requirement of 3.20;
- compile at least one MSVC configuration with `/permissive-`;
- add a second compiler job so MSVC extensions cannot silently become required.

Passing `/std:c++20` in an ad hoc flags variable is useful for an audit but should
not be the committed implementation.

## Build experiments

The audit used CMake 4.4.3, MSVC 19.44.35228, Windows SDK 10.0.22621, the existing
vcpkg installation, and a fresh out-of-tree Debug build with `BUILD_TESTS=ON`.
The final successful diagnostic build retained MSVC's normal `/EHsc` exception
mode.

| Experiment | Result |
|---|---|
| Plain MSVC `/std:c++17` | Failed on ambiguity between C++17 `std::byte` and the Windows SDK's global `byte`. |
| C++17 with `std::byte` temporarily disabled | Reached the executable and failed on the one removed `std::auto_ptr`. |
| C++17 with `_HAS_STD_BYTE=0` and `_HAS_AUTO_PTR_ETC=1` probes | The complete target graph built; all six CTest entries passed. |
| C++17 plus `/permissive-` | Exposed 81 distinct legacy string-literal/`char*` error sites in `VS_UI` before the dependent game target could build. |
| C++20 with the two library probes | Exposed 126 unique string-literal/`char*` error sites across 24 files: 81 in `VS_UI`, plus 48 in executable sources, with three shared header sites counted once. |
| C++20 plus `_HAS_STD_BYTE=0`, `_HAS_AUTO_PTR_ETC=1`, and `/Zc:strictStrings-` probes | The complete target graph built; all six CTest entries passed. The unit binary reported **375 tests, 5,066 checks, 0 failures**. |
| Clang 19.1.5 C++17 compile probe | Rejected non-empty dynamic exception specifications immediately; one representative packet translation unit hit Clang's 20-error limit in `SocketAPI.h`. |
| Clang 19.1.5 SpriteLib probe | Rejected `register` declarations as invalid ISO C++17. |

The `_HAS_*` macros and `/Zc:strictStrings-` were used only to expose the next
layer of errors. They are **not proposed fixes**: they disable new library features
or retain non-conforming source behavior.

Only Debug was compiled in this audit. Release, ASan, and in-game behavior remain
acceptance work for the implementation.

## Findings

### 1. C++17 `std::byte` conflicts with global namespace imports

`Client/Client_PCH.h:30` contains `using namespace std;`. Some translation units
then include Windows headers such as `wtypes.h`, whose unqualified `byte` collides
with `std::byte`. `Properties.cpp` and `StringStream.cpp` reproduce this failure.

A repository scan found 65 `using namespace std` directives in 63 files. Fifty
matches are in headers; two of those are commented out, leaving approximately 48
active header-level imports. The precompiled header is the high-impact instance
because it affects most client translation units.

The preferred fix is to remove namespace directives from headers and qualify the
standard-library names they expose. A Windows-only `_HAS_STD_BYTE=0` definition is
a small compatibility shim, but it hides rather than fixes the namespace problem
and prevents use of a standard C++17 feature.

### 2. One removed standard-library type is used

`Client/PacketHandler/GCMonsterKillQuestInfoHandler.cpp:30` has the repository's
only `std::auto_ptr` occurrence. It owns the raw pointer returned by
`popQuestInfo()`, so replacing it with `std::unique_ptr` should be local and low
risk.

The scan found no uses of the other common C++17/C++20 removals: `random_shuffle`,
`bind1st`/`bind2nd`, `mem_fun`, `std::iterator`, `result_of`, old allocator member
functions, `uncaught_exception`, or `get_temporary_buffer`. It also found no `u8`
string literals that would trigger the C++20 `char8_t` type change.

### 3. Dynamic exception specifications are the main conformance workload

The packet subsystem uses old specifications on declarations and definitions; for
example, `Client/Packet/SocketAPI.h:78-203`. A syntax-focused scan found:

- **3,304 non-empty type-list specifications** such as
  `throw(ProtocolException, Error)`;
- **1,317 files** containing those non-empty specifications;
- **8,655 empty specifications** written as `throw()`.

The non-empty form was removed in C++17. MSVC continues to accept and ignore it as
an extension (the existing C4290 warning family), which is why a permissive MSVC
C++20 build can succeed. Clang rejects it.

The safe mechanical mapping for the current Windows behavior is:

```cpp
throw()                         -> noexcept
throw(ProtocolException, Error) -> noexcept(false)
```

MSVC already ignores the list of permitted exception types, so `noexcept(false)`
matches the effective supported-platform behavior more closely than attempting to
recreate the old runtime type filter. Declarations, definitions, base classes, and
overrides must be changed consistently because exception specifications are part
of the function type in modern C++.

This edit is scriptable, but it touches most of the packet tree and therefore has
substantial review, merge-conflict, and clean-build cost. A syntax-aware rewrite
or tightly constrained pattern is required so parenthesized `throw` expressions
and comments are not altered.

There is a related pre-existing correctness concern: MSVC reports functions
declared `throw()` whose bodies can throw. Converting `throw()` to `noexcept`
preserves today's termination behavior; deciding that those functions should
propagate instead is a separate behavioral audit and should not be mixed into the
language port.

### 4. `register` remains in C++ source

There are roughly 650 declaration-like uses of the removed `register` storage
specifier. They are concentrated in SpriteLib drawing loops, `MTopView.cpp`, and
several UI loops. Twenty-six textual occurrences are in `.c` files and do not need
to change because those files remain C.

MSVC accepts the C++ occurrences as extensions. Clang 19 rejects them in C++17.
Removing `register` is mechanical and has no intended runtime effect, but it must
be included for an ISO-clean result.

### 5. C++20 makes legacy string-literal conversions hard errors

With the two removed-library issues bypassed, MSVC C++20 reported at least **126
unique error sites across 24 files**. These are not 126 independent designs:

- `basic/BasicException.h:30` declares read-only error text and `__FILE__` as
  mutable `char*`; making the declaration and definition const-correct resolves
  many repeated sites.
- Tables such as `VS_UI/src/VS_UI_GameSlayer.cpp:1100`,
  `Client/CrashReport.cpp:33`, and `Client/md5.cpp:30` store string literals in
  mutable pointer types. Most should be arrays of `const char*`.
- Several APIs accept `char*` even when their implementations appear to read only,
  including help-key lookup, chat text, login-ID backup, opening-video paths, and
  UI cursor setup.

Every affected API must be checked before changing its parameter to `const char*`.
Using `const_cast` or retaining `/Zc:strictStrings-` would conceal a real contract
problem and is not an acceptable final fix.

Strict C++17/Clang validation will expose the same const-correctness debt even
though default MSVC C++17 still permits many of the conversions.

### 6. C++20 adds little after the shared cleanup

Once `std::byte`, `auto_ptr`, and strict string literals were temporarily handled,
the entire MSVC C++20 graph compiled and all tests passed. The static scan also
found no live identifiers conflicting with `concept`, `requires`, `module`, or the
coroutine keywords, and no additional removed standard-library APIs.

This does not prove there are no errors hidden behind the known failures. Only a
complete Clang build after the exception-specification and `register` sweeps can do
that. A 25-35% contingency is included in the ISO-clean estimate for such
second-order failures.

### 7. Automated coverage is useful but not sufficient

The six CTest entries cover the unit binary plus architecture, formatting, packet
index, and generated-inventory checks. The unit binary exercises the static
libraries, including `packetwire` and `gamemodel`, and its wire goldens are valuable
because the port should not change packet bytes.

`tests/CMakeLists.txt` explicitly records the main limit: game logic compiled
directly into `DarkEden`, including packet handlers, cannot be linked into the unit
test binary. The language change therefore also needs an in-game smoke test against
a live server. Compile success alone cannot validate UI string lifetime, exception
paths, login, zone loading, chat, or shutdown.

There is no checked-in CI workflow, so compatibility could regress immediately
unless the new language mode becomes the only supported build contract or is
enforced in CI.

## Estimated work breakdown

The following is the recommended ISO-clean C++20 scope. Some tasks overlap, so the
total is a range rather than a direct sum of maximums.

| Workstream | Effort | Notes |
|---|---:|---|
| Build contract and compiler matrix | 1-2 days | CMake minimum/standard, extensions off, strict MSVC job, Clang job, Debug and Release. |
| `std::byte`, namespace hygiene, and `auto_ptr` | 1-3 days | The one ownership change is trivial; removing header namespace pollution determines the range. |
| Const-correct string interfaces | 2-4 days | At least 126 observed sites in 24 files; several collapse to shared API fixes, but mutation contracts need review. |
| Exception-specification migration | 4-7 days | About 11,959 total specification sites, 3,304 of them illegal type lists, spread over 1,317 files. Includes a codemod, declaration/definition repair, and review. |
| Remove C++ `register` and fix remaining strict-compiler findings | 2-4 days | Roughly 650 candidates plus issues revealed only after the dominant blockers are gone. |
| Verification and stabilization | 3-5 days | Clean Debug/Release builds, ordinary and ASan tests, wire checks, startup/login/zone/chat/UI/shutdown smoke tests. |

Expected total: **15-25 engineer-days**. A reviewer familiar with the packet
generator-style files can keep the work near the low end. Discovering behavioral
exception issues or cross-platform requirements would push it beyond the range.

The MSVC-extension estimates are shorter because they deliberately skip the
exception-specification and `register` sweeps. That is a valid staged milestone,
but it should be named "MSVC C++20 build" rather than "portable C++20."

## Recommended implementation sequence

1. Add a C++20 audit configuration and record the current failure counts. Keep the
   existing C++11 build green during the first fixes.
2. Replace the single `auto_ptr`, correct the `std::byte` namespace collision, and
   fix the const-string API families. Do not commit the diagnostic `_HAS_*` or
   `/Zc:strictStrings-` switches as solutions.
3. Land the exception-specification conversion in reviewable subsystem batches.
   Add a zero-growth/zero-target ratchet so old `throw(Type)` syntax cannot return.
4. Remove C++ `register` uses, leaving C sources alone. Add the same kind of
   ratchet for C++ files.
5. Turn on C++20, `CMAKE_CXX_EXTENSIONS OFF`, and strict MSVC compilation as the
   normal build. Build every target from a clean directory.
6. Add Clang compilation. The local machine has a Clang 19 executable, but the
   Visual Studio ClangCL toolset integration is not installed, so this audit could
   run representative compile probes rather than a complete Clang target graph.
7. Run all tests in ordinary and ASan trees, add Release coverage, then perform the
   live-server smoke test before declaring the port complete.

## Acceptance criteria

The port is complete when all of the following are true:

- CMake requests C++20 directly and refuses an older standard;
- no diagnostic feature-disabling macros or permissive string-literal switch are
  required;
- no non-empty dynamic exception specification remains;
- no `register` specifier remains in C++ source;
- all configured libraries, tools, tests, and `DarkEden` build cleanly in Debug and
  Release with the supported MSVC toolset;
- a second compiler builds the intended portable subset or complete Windows graph,
  depending on the agreed support promise;
- all CTest checks and the unit suite pass in ordinary and ASan builds;
- packet wire inventories and golden bytes are unchanged;
- startup, login, zone entry, UI interaction, chat, and clean shutdown are verified
  against a live server.

Modernizing unrelated raw pointers, containers, rendering code, platform APIs, or
the remaining warning backlog is explicitly outside these criteria.

## Post-migration C++20 modernization backlog

These items are not required to declare the language-mode migration complete.
They are follow-up opportunities that use the new baseline to reduce defects and
maintenance cost. PR #83 establishes the Windows/MSVC C++20 build milestone; the
items below should be delivered separately in small, reviewable subsystem PRs.

The occurrence counts are approximate static-scan results recorded on 2026-09-04.
They indicate where to investigate, not how many mechanical replacements are safe.

| Priority | Opportunity | Likely payoff | Suggested first slice | Size |
|---:|---|---|---|---:|
| 1 | `std::source_location` for diagnostics | Records file, line, and function without manually forwarding `__FILE__` and `__LINE__`. | Adapt `basic/BasicException.h` and `basic/DebugLog.h` while retaining compatibility wrappers for existing call sites. | Small |
| 2 | C++20 container and string helpers | `contains`, `starts_with`, `ends_with`, and `std::erase_if` express intent and remove repeated iterator boilerplate. | Convert a few table/manager membership checks and add focused tests; do not mix this with behavioral changes. | Small |
| 3 | `std::span` at packet and buffer boundaries | Couples a buffer with its extent and prevents pointer/length disagreement. The scan found roughly 217 pointer-plus-size interfaces. | Add span overloads to `SocketInputStream` and `SocketOutputStream`, keep the old overloads as adapters, and migrate one packet family. | Medium |
| 4 | Typed packet serialization with concepts | Constrains wire reads/writes to supported fixed-width integral and enum types, producing clearer compile-time errors. | Introduce an internal `WireScalar` concept plus size assertions without changing existing packet bytes or public packet IDs. | Medium |
| 5 | `std::chrono::steady_clock` and typed durations | Avoids unit confusion and the rollover-prone `previous + delay <= now` pattern. About 259 `GetTickCount`/`timeGetTime` calls remain. | Add a central monotonic-clock adapter, then migrate `basic/Timer2` and one UI timer with wraparound tests. | Medium |
| 6 | `std::filesystem` for path and directory work | Removes manual search-handle lifetime and path-buffer handling. About 34 Win32/CRT enumeration calls remain. | Migrate profile discovery or log cleanup first, preserving filename ordering, case handling, and wildcard behavior. | Medium |
| 7 | Bounded modern formatting | Reduces format/argument mismatches and destination-buffer mistakes. The scan found about 628 printf-family and 443 `strcpy`/`strcat` calls. | Use `std::format` or `format_to_n` only for developer-owned logging strings at first. | Large/staged |
| 8 | `std::jthread` and `std::stop_token` for owned workers | Makes join and cancellation responsibilities explicit and exception-safe. Raw Win32 thread/synchronization primitives appear in about 36 locations. | Prototype on one clearly owned worker; preserve required Windows event and message-loop integration. | Large/staged |
| 9 | Ownership RAII | Replacing proven owning raw pointers with `unique_ptr` prevents leaks and partial-initialization cleanup bugs. This is not C++20-specific, but the migration makes it a natural follow-up. | Inventory ownership in one manager and convert only pointers with unambiguous single ownership. | Large/staged |

**Source-location status (2026-09-05):** priority 1 is implemented for both
diagnostics facilities. `basic/BasicException.h` gains an `ExceptionSite` whose
default constructor captures the caller through a defaulted
`std::source_location::current()`, plus a `g_BasicException(code, sz_error,
site)` entry point; `_Error`, `_ErrorStr` and `CheckMemAlloc` no longer spell
`__FILE__` and `__LINE__` out, and the `(code, sz_error, file, line)` function
stays as a compatibility wrapper for call sites that name a location
explicitly. `basic/DebugLog.h` gains the equivalent `LogSite` and a
`log_write_at(site, level, fmt, ...)` entry point, with the site in front of
the format because a `std::source_location` cannot follow a C variadic `...`;
the `LOG_*` and `DEBUG_ADD*` macros are untouched and both entry points share
one core. The log line written to console and file is unchanged - the function
name is captured but not printed. `tests/unit/test_source_location_diagnostics.cpp`
pins every capture against the test file's own `__FILE__` and `__LINE__`, which
is what proves the nested defaulted `current()` reports the call site rather
than the header. No call site elsewhere in the tree is converted; that is a
later slice.

**Source-location second slice (2026-09-05):** the explicit `__FILE__` and
`__LINE__` forwarders in the packet wire layer are converted, so the library
that carries them is also the one a test binary can link.
`Client/Packet/Exception.h` gains `DiagnosticSite` - the same shape as
`ExceptionSite` and `LogSite`, a separate type only because the wire layer's
exception header may not drag in the `EXCEPTION_CODE` enum and the
`_Error` macros those carry - and a `Throwable::addStack(site)` overload that
defaults it; `__END_CATCH`, which
wraps essentially every method in the tree, now calls `addStack()` and pushes
the identical `"file:line"` frame. `Client/Packet/PacketAssert.h` gains
`__assert__(func, expr, site)` and its `Assert(expr)` macro forwards only the
two things a location cannot carry, the platform's function spelling and the
stringized expression; the four-argument `__assert__` stays as the
compatibility entry point and is what the new one delegates to, so the line
written to `assertion_failed.log` and the message inside the thrown
`AssertionError` are byte for byte what they were, empty-function-name
separator quirk included. Pinning that text exposed a pre-existing defect,
now under *Found by reading* in the code-health review:
`StringStream::operator<<(char)` appends a NUL after every streamed
character, so every bug report the client sends the server is cut off
before its stack trace; the tests spell the NUL out rather than hide it, and
a `fix:` slice follows. `Client/Packet/Assert1.h`, an unreferenced duplicate
of `PacketAssert.h` carrying the same include guard, is kept in step rather
than left to rot. `Client/Packet/ClientPlayer.cpp`'s packet-skip notice
repeated its file and line inside a message the log header already stamped
with them; both halves now read one captured `LogSite`, and the site is
captured on the line the macro occupied, so the text is unchanged. No macro
name changed, so nothing under `Client/PacketHandler` needed editing.
`tests/unit/test_packet_source_location.cpp` pins each converted shape against
the test file's own `__FILE__` and `__LINE__`, and pins the packet-skip line
by writing the old spelling and the new one into the log file sink and
comparing both. Left alone: `Client/Packet/SocketOutputStream.cpp`'s
`printf`, which is not a forwarder - it prints its own location, so a capture
there would only respell the same two tokens. Everything executable-side
(`Client/PacketFunction.cpp`, `Client/MEffectGeneratorTable.cpp`, the
`GCSkillToTileOK` handlers, `Client/LeakMemoryDumper.*`,
`Client/DebugInfo.h`, `VS_UI/src/Imm/IFCErrors.h`) is a later slice.

**Container-helper status (2026-09-05):** the first priority-2 slice is
implemented. Twelve membership and line-trimming sites in library code -
`PacketIDSet`, `GCTimeLimitItemInfo`, `GCNPCAskVariable`, `Properties`,
`SystemAvailabilities` and the item, sorted-item, time-item and skill-domain
managers - are written as `contains`, `starts_with` and `ends_with`. Every
converted site is reached through a public entry point by
`tests/unit/test_cpp20_container_helpers.cpp`, which was run against the
pre-conversion sources as well and gave the same result. No container type
changed, no packet byte changed, and the candidates that were not equivalent
(an erase of one element where `std::erase` would remove all matches, and a
linear scan over a map by `operator==`) are listed in the commit and left
alone.

**Span status (2026-09-04):** the first priority-3 slice is implemented in PR
#84. `SocketInputStream` and `SocketOutputStream` now expose
bounded `std::span<char>` and `std::span<std::byte>` overloads while retaining the
pointer/length APIs as adapters. `GCExchangeList` is the representative migrated
packet: its seven length-prefixed strings and two raw 64-bit fields use spans, and
a pre-migration golden pins the complete body byte-for-byte. Future slices should
migrate small packet families behind equivalent round-trip/golden coverage rather
than expanding this into a mechanical whole-tree rewrite.

**Typed-scalar status (2026-09-04):** the first priority-4 slice is implemented
in PR #85. The stream layer accepts only exact-width signed
and unsigned integers, or enums backed by those types, through constrained
`readWire`/`writeWire` entry points. Existing plain and encrypted scalar overloads
remain compatible and delegate to that implementation; ambiguous `bool` and plain
`char` behavior is deliberately unchanged. The Exchange buy/list 64-bit identifiers
and the ordinary `CGAttack` combat path are the representative migrations,
protected by client/server-shared goldens across every encryption code. Later
slices can migrate remaining raw scalar casts family by family without widening
the accepted type set.

**Clock status (2026-09-05):** the first priority-5 slice is implemented.
`basic/MonotonicClock.{h,cpp}` is the central adapter: `Now()` is
`std::chrono::steady_clock` truncated to milliseconds, `Duration` is
`std::chrono::milliseconds`, and `SetTestSource()`/`ScopedTestSource` inject a
deterministic clock so timing tests are neither sleeps nor flakes.
`LegacyTicks()` is deliberately **not** derived from `Now()`: on the real clock
it is `platform_get_ticks()`, so a class that is only half converted keeps
comparing against the same epoch it always did, and later slices can migrate
call sites one at a time. `basic/Timer2` is migrated internally with its public
`C_TIMER2` API - `DWORD` milliseconds, `timer_id_t`, `INVALID_TID` - unchanged,
and `C_VS_UI_TITLE::Timer()` (the title-screen credit scroll) is the
representative UI site. `tests/unit/test_monotonic_clock.cpp` and
`tests/unit/test_timer2.cpp` pin the firing rule and drive the injected clock
across the 32-bit tick's wrap and past 2^32 ms of elapsed time.

Reading the code for this recorded one thing that is worth knowing before the
next slice: on `PLATFORM_WINDOWS`, `Platform.h` redefines only `timeGetTime()`
as `platform_get_ticks()`. `GetTickCount()` is still kernel32's, so the client
already reads two counters that share neither epoch (since boot versus since
SDL init) nor resolution (about 15.6 ms versus 1 ms), and nothing in the source
marks which sites use which. Moving a site off `GetTickCount()` therefore also
takes it off the 15.6 ms quantisation, which is a small change in when it fires
and has to be stated each time it is made.

**Filesystem status (2026-09-05):** the first priority-6 slice is implemented.
`basic/DirectoryListing.{h,cpp}` lists a directory through
`std::filesystem::directory_iterator` against a DOS-style wildcard and returns
a snapshot sorted in the case-insensitive ordinal order an NTFS `_findnext`
walk produced, so a caller owns no search handle and cannot be handed a file it
created inside its own loop. The header carries a measured semantics table:
`*` and `?` matching, case-insensitive ASCII folding and the NTFS order are
preserved; 8.3 short-name aliases, `.` and `..`, and Win32's zero-or-one
trailing `?` are deliberately not. Profile discovery and deletion in
`Client/ProfileManager.cpp` and the log cleanup in `Client/Client.cpp` are the
migrated walks; the latter also drops a `long` that truncated the CRT's
`intptr_t` search handle on x64. `tests/unit/test_directory_listing.cpp` pins
the matcher, the order and the failure contract over a scratch directory. The
migrated callers are executable-only and need the runtime check that the login
screen still lists profiles. A second slice migrated the last three `_findfirst`
walks: the one that empties the `Update` directory in `Client/Client.cpp`, and
`UUFDeleteDirectory` and `UUFDeleteFiles` in `Client/Updater/UpdateUtility.cpp`,
all of which stored the CRT's `intptr_t` search handle in a `long`. Each keeps
its `_chdir` dance and its dotfile skip, asks for `*` where it asked for `*.*`,
and lists files only. `UpdateUtility.cpp` is compiled into no target and does
not build on its own for four pre-existing reasons in untouched functions; the
migrated code was proven to compile with those patched temporarily. No
`_findfirst` call remains in live code. A third slice took the four
`FindFirstFile` call sites that were left: the `Updater2.exe` existence test
in `Client/Client.cpp`, which never closed its search handle and is now a
`std::filesystem::exists` through the `std::error_code` overload; the startup
DLL whitelist in the same file; the older copy of that whitelist in
`Client/GameInit.cpp`, whose `g_wAuthKeyMap` assignment fires only when the
listing is non-empty and every name on it is whitelisted; and
`C_VS_UI_FILE_DIALOG::RefreshFileList` in `VS_UI/src/VS_UI_ExtraDialog.cpp`,
the profile-picture file dialog. The whitelist keeps its silent `return -1`
out of `WinMain` - no message box and no log line, because logging is not up
yet - and both whitelists list directories as well as files, because they
judge the name alone and a subdirectory whose name ends in `.dll` used to
reject the client too. The 8.3-alias deviation applies to them: a file such
as `foo.dllx` was matched through its `FOO~1.DLL` short name and stopped
startup, and is no longer listed, so the check only ever gets looser and no
name that was listed stops being listed. The file dialog synthesises the `..`
entry `directory_iterator` never yields, first in the sequence and only for a
directory that has a parent - `path::has_relative_path()` decides that, and
`dir /a` confirms it, listing `..` for `C:\Users` and not for `C:\`. Reading
that function turned up a pre-existing defect left for its own fix: the inner
`for(int i = 0; ...)` shadows the outer `i`, so the
`if(i == m_vs_file_list.size())` after it tests the filter loop's counter
instead, and since the inner loop never breaks for a file, every file is
dropped unless `m_filter.size()` happens to equal the list size. The `..`
entry is synthesised outside the success branch, because the helper is all
or nothing where `FindFirstFile` had delivered `..` before any `FindNextFile`
could fail; the same all-or-nothing rule is a deviation of the two whitelists
too, and there it can only skip the check. A dangling junction is the one
entry the helper classifies differently from `FindFirstFile`: it follows the
link and reports a file where the legacy walk reported the reparse point's
own directory bit, so the dialog no longer shows one.
`tests/unit/test_directory_listing.cpp` now also pins the trailing-separator
directory string the dialog is the only caller to pass. No `FindFirstFile` call
remains in live code - only the commented-out loop in
`VS_UI/src/VS_UI_Tutorial.cpp` - and the `FindFirstFileA`, `FindNextFileA`
and `FindClose` shims in `basic/Platform.h` now have no caller either, so
they can go with the `_findfirst` ones in a later sweep.

### Packet modernization guardrails

Packet I/O is the highest-value area but also the easiest place to cause a silent
compatibility break. A span/concepts change should:

- retain the existing packet wire inventory and golden-byte tests;
- use fixed-width wire types and add compile-time width checks;
- make byte order explicit with `std::endian` where multi-byte values cross the
  network boundary;
- avoid serializing whole C++ structs or using `std::bit_cast` across padding;
- add new overloads before migrating call sites, rather than changing every packet
  in one patch.

### Formatting guardrails

`basic/SafeFormat` intentionally tolerates malformed or data-controlled legacy
format strings. A blanket conversion to `std::format` would change that behavior
and may introduce exceptions. Keep `SafeFormat` for localized or server/data-owned
formats unless their grammar and failure policy are deliberately migrated. Begin
with compile-time, developer-owned log messages and bounded destinations.

### Suggested delivery order

1. Land `source_location` diagnostics and the simple C++20 container/string helpers.
2. Add a backward-compatible span layer to packet streams and migrate one packet
   family with unchanged wire goldens.
3. Add typed wire helpers and concepts after the span boundary is established.
4. Migrate clocks and filesystem operations one subsystem at a time.
5. Treat formatting, thread ownership, and raw-pointer ownership as dedicated
   stabilization projects with their own tests and runtime smoke checks.

Modules, coroutines, mass ranges rewrites, blanket `char8_t` conversion, and broad
syntax-only modernization are deliberately not priorities. They offer less defect
reduction than the boundary, lifetime, timing, and diagnostics work above.
