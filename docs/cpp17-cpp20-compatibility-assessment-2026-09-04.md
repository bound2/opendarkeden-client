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

**Build-contract status (2026-09-07):** `CMAKE_CXX_STANDARD 20` with
`CMAKE_CXX_STANDARD_REQUIRED ON` was already the contract (PR #83), and the
root `CMakeLists.txt` now adds `CMAKE_CXX_EXTENSIONS OFF` and, for MSVC,
`/permissive-` and `/Zc:__cplusplus` on every C++ target through
`add_compile_options`, next to `/MP`. With findings 3 and 4 closed the whole
tree - 1,241 translation units, the tests included - builds under
`/permissive-` with **0 errors**, and every test passes in the plain and the
ASan tree, so there was no strict-mode workload left to schedule: the
string-literal fixes of finding 5 and the exception and `register` sweeps
were the whole of it. One trap, recorded because the first probe fell into
it: passing `/permissive-` in `-DCMAKE_CXX_FLAGS=` on the configure line
replaces CMake's MSVC defaults, `/EHsc` among them, and without `/EHsc` a
C++ exception cannot be caught by type - six tests failed with "uncaught
exception of unknown type" and the receive-loop test saw packets leak on the
throw path, none of which had anything to do with conformance. The committed
change adds the option and keeps the defaults. The second-compiler job
followed the same day: the `windows-clang` preset builds the whole tree with
clang-cl 19 through NMake from a VS developer prompt (the ClangCL toolset is
not installed, but the compiler is), and the experiments table below has the
result. A Linux or macOS build is a port, not a language-mode question.

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
| clang-cl 19.1.5, whole tree, 2026-09-07 (NMake, the `windows-clang` preset) | With findings 3 and 4 closed: every library, every tool, `unit_tests` and `DarkEden` compile and link; the clang-built unit binary reports **596 tests, 294,382 checks, 0 failures** and every ctest passes. Two ISO defects MSVC had accepted as extensions were found and fixed: a `POINT` brace-initialised from `0xFFFFFFFF` (narrowing to `LONG`), and an `enum` whose first value `0xffff0000` gave it `int` as its underlying type, so that its members were negative `case` labels against an unsigned `id_t`; it now has `unsigned int` as its underlying type. |

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

**Conformance status (2026-09-06):** the first slice of this finding was scoped
to `basic/` and **removed nothing, because `basic/` has never carried a dynamic
exception specification**: 0 before, 0 after. That is not a quirk of the smallest
library. Measured with the pattern R9 and R10 now use - `throw` followed by
parentheses holding type names or nothing, files joined so a wrapped type list
still counts, `//` tails stripped - **every library in this tree is already at
0**: `basic`, `Client/SpriteLib`, `Client/TextSystem`, `Client/DXLib`,
`Client/framelib`, `VS_UI` and every `gamemodel` member. The entire workload is
the packet tree and what hangs off it. The repository holds **11,790**
specifications in `.h` and `.cpp` files outside comments (a first measurement
that did not strip `/* */` blocks read 11,841; the 51 it over-counted were dead
comment text), 8,513 empty and 3,277 type lists, split as: **11,463** in the
library set (2,211 in `packetwire`'s own `.cpp`, 9,252 in the headers under
`Client/Packet`, over the set's 1,473 files), **284**
in `Client/PacketHandler`, **34** in the remaining executable sources - the two
request-side packet factory managers, `PacketFunction.cpp`, `RequestFileManager`
and `Updater/UpdateManager.h` - and **9** in `tests/`, in test doubles that
implement wire interfaces and have to match the specifications on their bases.
The tree's 45 `.c` files carry none, so unlike `register` in finding 4 there is
nothing here that can be left alone for being C. This restates finding 3's own
scan (3,304 non-empty, 8,655 empty, 1,317 files), taken with a different pattern
over a scope the audit did not record; the two agree to within about 1% and the
shape of the conclusion is unchanged.

The slice therefore shipped the instrument rather than an edit.
`tests/ratchet/ratchets.sh` gains two baselines: **R9 = 0** over `basic/`, which
holds the line there, and **R10 = 11,463** over the library set, which later
slices ratchet down. R10's set deliberately includes every `.h` under
`Client/Packet` even though `tests/arch/packetwire_files.txt` lists only `.cpp`,
because an exception specification is part of the function type and a
declaration and its definition must change together - a `.cpp`-only metric would
have counted exactly half of every edit as progress. Both were injection-tested
in both directions: adding `throw ( Error )` or `throw ()` to a `basic/` header
fails them, `noexcept` and the `throw Error(...)` / `throw ("...")` statements do
not, and removing one specification from `SocketAPI.h` fails R10 as unrecorded
progress. `Client/PacketHandler` and the rest of the executable are outside both,
and adding a ratchet for them belongs with the slice that clears them.

A ratchet is the only instrument available here, because the build says nothing.
The Debug log of this tree at `/std:c++20` contains **C4290 zero times** - the
warning the project notes name for the packet tree - so not one of the 11,499
sites shows up as a diagnostic. What the build does emit is **539 distinct
C4297**, "function assumed not to throw an exception but does", across 257
`Client/Packet` files. That is the pre-existing correctness concern this finding
raises, now with a number: those are `throw()` functions whose bodies throw,
already undefined under MSVC's own reading of the specification, and they are the
set where `throw()` must become a dropped specification rather than `noexcept`.

**The next slice is `packetwire`, because there is no smaller library left.** It
is the whole 11,463 and cannot land in one commit; the natural subdivision is the
one `Client/Packet` already has - the root files first (the exception header the
`__BEGIN_TRY`/`__END_CATCH` pair comes from, the streams, the sockets and the
framing, which is where the declarations everything else overrides live), then
the packet directories one at a time. Three things found while measuring should
travel with those slices.

Dropping `throw(X, Y)` is mechanical: MSVC ignores the list already, so removing
it changes nothing MSVC does. `throw()` -> `noexcept` is not, and it is a
judgement per function. MSVC already treats `throw()` as nothrow and omits the
unwinding, so a `throw()` function whose body can throw is undefined today -
which means the specification should be **dropped** wherever the body cannot be
shown nothrow, and promoted to `noexcept` only where it can. The tree already
carries one worked example of that reasoning, at `Client/Packet/WireHost.h:155`:
`RequestClientPlayer::readInputStream` and `RequestServerPlayer::send` are
declared `throw(ProtocolException, Error)` on purpose, because the peer teardown
depends on that exception unwinding to `RequestServerPlayerManager::Update`, and
a `throw()` there would make the path undefined under MSVC and `std::terminate`
under C++17 or on clang and gcc.

`SocketAPI.cpp` writes five of its specifications across two lines and repeats
each one in the banner comment above the function, so a line-oriented codemod
will edit the code and leave the comment claiming the old contract. One of those
comments is already wrong today: `bind_ex`'s names an `MBindException` that both
the declaration and the definition spell `BindException`.

Finally, the nine specifications in `tests/` are not debt of the same kind. They
sit on test doubles that override wire interfaces
(`test_output_stream_flush.cpp`, `test_player_base.cpp`) and mirror what those
bases declare, so they belong in the same commit as the base they mirror. The
`throw()` half of that is the part that cannot be deferred: once a base is
`noexcept`, an override that is not stops compiling.

**Packet-root status (2026-09-07):** the 162 files directly under
`Client/Packet` are at 0 - 143 of them carried a specification, 1,799 in
all - and so is `tests/`; **R10 = 9,664**. The work went as
four slices - the wire core (the exception header, the asserts, the string
stream, the sockets, the datagram classes, the streams and the file API, 524
sites), the packet framework and the player classes (224), the
player-character info classes (558) and the remaining info classes (493) -
plus the nine in `tests/`, which moved with the bases they mirror. The rule
each slice applied, and the numbers it produced:

- **A type list is deleted, not respelled** - 643 of them. MSVC ignored the
  list already, and a function with no specification is what ISO C++ means
  by potentially throwing, so deleting it changes nothing MSVC does. The
  `noexcept(false)` this finding names above is the same thing said longer,
  and is not written anywhere except in the one place it is not the same
  thing: **a destructor**. A destructor with no specification is `noexcept`
  by default, so the ten that carried `throw(ProtocolException, Error)` -
  `Player` and its three subclasses, `Socket`, `SocketImpl`, `ServerSocket`,
  `DatagramSocket` and the two streams - are spelled `noexcept(false)`, which
  is what MSVC had read the list as. They can throw: the player destructors
  hold an `Assert` on the session state, and the socket ones call `close()`.
  The slices' first version deleted those lists too, and the compiler said
  so: no C4297 for any of the ten on master, C4297 for all ten once the list
  was gone.
- **An empty `throw()` is a judgement per function**: 599 became `noexcept`
  and 566 were deleted, 48 of the latter on destructors, which are `noexcept`
  by default anyway. Promoted: the scalar, pointer, enum and array-indexed
  getters and setters, the `getSize`/`getMaxSize` bodies that sum constants
  (`std::string::size()` and `std::list::size()` are `noexcept` by the
  standard), `clearList()` over `std::list::clear()`, and the bitset setters
  in the slayer outlooks. Deleted: anything returning or assigning a
  `std::string` or a container by value (every `getName`, `setName`,
  `toString`, the `*Info3` copy constructors), anything wrapped in
  `__BEGIN_TRY`/`__END_CATCH` (the pair rethrows), `new`, `front()` and
  `pop_front()`, the bitset getters (`to_ulong` and `test` throw), and the
  exception-class constructors - `Throwable` holds a `std::list<std::string>`
  and MSVC's `std::list` default constructor allocates its sentinel node and
  is not `noexcept`, so `Throwable()` and the 41 default constructors that
  chain to it are deleted rather than promoted, though libstdc++ and libc++
  would have allowed it.
- **A virtual is promoted only when every override in the repository is
  `throw()` or `noexcept` and passes the same test**, and the slices checked
  by grep before promoting: `PCInfo::getPCType` and `PCInfo::getSize` (nine
  overrides, all in the slice), and the three `getSize` overrides of
  `PCSkillInfo` (whose base is deleted, since those overrides walk a list).
  Deleted rather than promoted, on purpose: `PacketFactory::getPacketID` and
  `getPacketMaxSize` (448 factory subclasses in the packet directories, still
  `throw()` and rewritten by later slices; `GCPetStashListFactory` and
  `GCGoodsListFactory` already call an unspecified `getPacketMaxSize`),
  `DatagramPacket`'s pure virtuals, `ModifyInfo::getPacketSize` (not
  virtual, but about thirty `Gpackets` classes declare a member of the same
  name that hides it) and `WarInfo::getSize`, whose overrides add
  `ValueList::getPacketSize`. A base with no specification compiles under any
  override, which is the property the packet-directory slices need.
- **The two things this finding said should travel with the slice did**:
  `SocketAPI.cpp`'s five two-line specifications are gone, along with the
  copies in the banner comments above them and the `MBindException` name one
  of those had wrong; and the `WireHost.h` comment that said `readInputStream`
  and `send` "are throw(ProtocolException, Error)" now says they propagate
  those exceptions.

Two facts about the files that the next slices should know. The packet-root
files are **CRLF in the working tree** on this machine, and `Datagram.h` and
`SocketInputStream.h` are stored with mixed endings; `grep -c $'\r'` reports
0 for many of them and is not a line-ending check here - count bytes with
perl, compare the changed-line count of `git diff --numstat` with the
specification count per file, and expect the Edit tool to normalise a mixed
file. And the compiler is a usable oracle for one direction: a `throw()`
promoted to `noexcept` whose body can throw draws C4297, so a promotion that
adds a C4297 to the build is wrong, while the C4297s that remain on the
destructors wrapped in `__BEGIN_TRY` are the pre-existing concern this finding
raises and not this slice's to settle.

Verified: unit_tests in `build/tests` and `build/tests-asan`, 596 tests,
294,382 checks, 0 failed in both; DarkEden in `build/vs2022` with 0 errors,
which is the check that matters for the headers every handler includes; the
wire inventory and every golden unchanged.

**Small packet directories (2026-09-07):** `Lpackets` (319 sites, 36 files),
`Upackets` (34, 4) and `Rpackets` (256, 22) are at 0 as well, and **R10 =
9,055**. The packet classes are regular enough that this slice was done by
script rather than by hand, with the same rule narrowed to what a script can
prove: a type list is deleted (163; the script aborts on a destructor carrying
one, and none does); a `throw()` becomes `noexcept` only on an inline
one-line member that returns a constant or a scalar or reference member with
no call in the expression, or assigns one scalar by-value parameter to a
member (162 - the `getPacketID`, `getPacketSize` and `getPacketMaxSize`
constants and the scalar getters and setters); every other `throw()` is
deleted (284 - the `getPacketName` and `toString` that return a
`std::string`, the `createPacket` that call `new`, the size functions with
out-of-line or calling bodies, the string setters, the list operations and
the eight destructors). Scalar means the fundamentals plus every typedef and
enum declared under `Client/Packet`, `Client/Packet/Types`, `basic/` and the
directory itself, so a `std::string` getter or a const-reference setter never
qualifies. The script's tokenizer copies comments and string literals
through, and a second tokenizer compared every file with its committed
version - every comment and literal identical, every code token identical
once the specifications and the `noexcept` tokens are erased. A body the rule
does not recognise is deleted, never promoted, so the script can only err by
leaving a nothrow function unspecified, which ISO C++ allows; the review of
its output is a review of the 162 promoted lines, which reduce to about sixty
distinct shapes. `Cpackets` (3,172) and `Gpackets` (5,883) are what remains,
and the same script applies to them.

**Cpackets (2026-09-07):** at 0, 3,172 sites in 326 files, **R10 = 5,883**.
The script's decisions were 873 type lists deleted, 1,049 `throw()` promoted
and 1,250 deleted, verified the same way (residual 0, every file compared
token by token with its committed version, the promoted lines reduced to
their shapes and read). The one thing the script cannot see is a packet
deriving from another packet: `CGUseMessageItemFromInventory` derives from
`CGUseItemFromInventory` and overrides its `getPacketSize` and its factory's
`getPacketMaxSize` with multi-line bodies the script deletes, while the base's
one-liners it promotes - and MSVC refused the pair, C2694, an override with a
less restrictive specification than its base. The base's two functions are
unspecified, by hand, and the build is the check that finds the next such
pair; `Gpackets` should expect the same for its `OK1`/`OK2`-style families.

**Gpackets (2026-09-07):** at 0, 5,883 sites in 516 files, and **R10 = 0**:
every library a test binary can link is free of dynamic exception
specifications, and the ratchet now holds the whole set at zero the way R9
holds `basic/`. The script's decisions were 1,293 type lists deleted, 1,845
`throw()` promoted and 2,745 deleted, verified as before; the promoted return
and parameter types were listed and read (the `PacketID_t`/`PacketSize_t`
constants, the `*_t` typedefs, the enums, the fundamentals, and const
references to `std::string` and the `PC*Info3` classes). Five packets derive
from another packet here; a scan of that shape ahead of the build found the
two that override a promoted one-liner (`GCMakeItemOK` and `GCMakeItemFail`
over `GCChangeInventoryItemNum::getPacketSize`), the base was left
unspecified by hand, and the build agreed. What this finding still owes is
outside R10: `Client/PacketHandler` (284) and the remaining executable
sources (34), which the slice that clears them should put under a ratchet of
their own, since a `throw()` there whose body throws is the same undefined
behaviour it was in the libraries.

**Executable side (2026-09-07): finding 3 is closed on the source side.**
The 319 specifications outside the library set are gone: 284 in
`Client/PacketHandler`, one on each handler's `execute` definition, all type
lists; 32 in the two request-side packet factory managers under
`Client/OtherClass`, whose constructors, destructors and `toString` had
`throw()` (deleted: `__BEGIN_TRY` bodies and a `std::string`) and whose
`getPacket`/`getPacketMaxSize` and validation entry points had lists; and
three lists in `RequestFileManager` and `Updater/UpdateManager.h`. Nothing
was promoted. The same script and the same token-by-token verification as
the packet directories; the only test path is the executable build, since
none of these files links into a test binary. Ratchet **R12 = 0** holds every
`.h`, `.cpp` and `.inl` under `Client`, `VS_UI`, `basic`, `tools`,
`third_party` and `tests` at zero, so R9 and R10 are now subsumed and kept
only as the record of how the libraries got there. What this finding still
does not settle is the pre-existing concern it raised at the top: the
`throw()` functions whose bodies throw were undefined under MSVC's reading and
now simply propagate, which is the behaviour ISO C++ gives an unspecified
function and the one the callers were written against; the C4297 count in
the build is what remains of it, on destructors wrapped in
`__BEGIN_TRY`/`__END_CATCH`, which are `noexcept` by default whatever is
written on them.

### 4. `register` remains in C++ source

There are roughly 650 declaration-like uses of the removed `register` storage
specifier. They are concentrated in SpriteLib drawing loops, `MTopView.cpp`, and
several UI loops. Twenty-six textual occurrences are in `.c` files and do not need
to change because those files remain C.

MSVC accepts the C++ occurrences as extensions. Clang 19 rejects them in C++17.
Removing `register` is mechanical and has no intended runtime effect, but it must
be included for an ISO-clean result.

**Conformance status (2026-09-07):** closed. The exact count, measured with
comments and string literals removed, was **627 declarations in 23 C++
files** - 584 of them in `Client/SpriteLib` (the blitters in `CSprite.cpp`,
`CIndexSprite.cpp`, the alpha, shadow, palette and surface variants,
`CFilter.cpp`, and one in `CTypePackVector.h`), 17 in `Client/MTopView.cpp`,
and the rest in `Client/CSpritePal.cpp`, `UtilityFunction.cpp`,
`MEventManager.cpp`, `MSector.cpp` and `VS_UI/src/vs_ui_gamecommon2.cpp`.
All 627 are deleted, seven of them written `int register i` rather than
`register int i`. The edit was scripted through a tokenizer that copies
comments and string literals through untouched, so the commented-out copies
of blitter loops that still say `register` inside `/* */` blocks are left as
they were, and a second tokenizer then compared every changed file with its
master version: every comment and literal identical, every code token
different only by the keyword and one run of blanks. That verifier exists
because the first pass of the sweep was run through a tokenizer whose string
class had been corrupted in transit and edited six NPC script lines in
`MNPCScriptTableEnglish.cpp` that say "register as a couple", "register a
team", "a clan" or "a guild"; the sweep was redone from master. The 21
declarations in the three `.c` files (`deflate.c`, `inftrees.c`, `trees.c`)
stay, as this finding said they should - this finding's "twenty-six in the
`.c` files" was a line count that included the prose in `crc32.c`'s comments,
which has no declaration at all. Ratchet **R11 = 0** holds the line over
every `.cpp`, `.h` and `.inl` under `Client`, `VS_UI`, `basic`, `tools`,
`third_party` and `tests`, counted by `tests/tools/count_register.pl`, which
also accepts the `WORD register i` spelling with any type name in front, not
only the seven `int register i` the sweep met; it does not reuse the R9/R10
comment-stripping pipeline, because that pipeline strips `/* */` before `//`
and so reads the blitters' `//*pDest = ...` line comments as block-comment
openers - measured over the 23 files on master it saw 538 of the 627, and
the figure moves with the order the files are joined in, because a swallowed
comment runs across file boundaries - and because it does not strip string
literals, so those six NPC lines would count as declarations to it. An
under-count is tolerable for a ratchet that must stay at zero only if nothing
can hide there. The same weakness applies to R10, where it can only
under-count, and is noted for the slice that next moves that baseline.

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
character, so every bug report the client hands `SendBugReport` was cut off
before its stack trace; the tests spelled the NUL out rather than hiding it,
and the `fix:` slice that removed it followed (`fix/stringstream-nul`, which
also took the NUL back out of these tests' expectations; `SendBugReport`'s
own cut then bounded what the server sees at 100 bytes, and `fix/bug-report-cut`
raised it to the 116 a `CGSay` message leaves once the `*bug_report ` prefix
has 12 of its 128).
`Client/Packet/Assert1.h`, an unreferenced duplicate
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

**Container-helper status, second slice (2026-09-06):** eleven more library
sites, in three files. `MSkillDomain::IsExistSkillStep` asks the step map with
`contains`; `MSkillDomain::AddSkillStep` asks its step list with
`std::ranges::find` instead of an index loop that set a flag;
`UseEnglishTextFrom` tests the language file's comment character and its
`LANGUAGE` keyword with `starts_with` over a `std::string_view` of the line, in
place of an index and an eight-byte `strncmp`;
`SystemAvailabilitiesManager::LoadFromStream` tests its four line kinds - `;`,
`*`, `Z`, `S` - the same way, with `empty()` where it called `strlen` on the
same buffer; `SystemAvailabilitiesManager::ZoneFiltering` and `CheckScript`
became `std::ranges::any_of` and `std::ranges::none_of` over the predicates
their hand written scans already used. Every converted site is reached through
a public entry point by `tests/unit/test_cpp20_container_helpers_2.cpp`, whose
six tests were built and run twice - once with the three sources reverted to
their committed state and once with the conversion in place - and reported
identical counts both times, all six passing in both; the stack this lands on
ends at 498 tests, 10,694 checks, 0 failed in both trees. One site has only
one reachable arm: `AddSkillStep`'s duplicate test is `true` on every call the
public API can make, because `AddSkill` calls it only for a skill not yet in
the domain, so the test that covers it pins the public contract rather than
the dead arm.

One candidate was converted, found untestable and reverted, and the reason is
worth keeping: `C_DIRECTORY::GetMixedDirectory` in `basic/Directory.cpp` tests
for a trailing path separator with `path[strlen(path) - 1]`, which `ends_with`
says directly - but a test that calls it does not link. `C_DIRECTORY`'s
constructor references `platform_get_executable_dir`, which `basic/PlatformSDL.cpp`
defines only under `#ifndef PLATFORM_WINDOWS`, so pulling `Directory.obj` into
`unit_tests` fails with `LNK2019`. The whole tree links today only because
nothing on the Windows build references `C_DIRECTORY` at all. That is a latent
defect rather than a compatibility one and is not fixed here; until it is,
`basic/Directory.cpp` has no test path and this slice left it alone.

Candidates read and rejected: every `find` whose iterator is
dereferenced afterwards, which is not a membership test (`MItemManager`'s
`GetItem` and `RemoveItem`, `MSkillSet`'s five accessors, `MSkillDomain`'s
status and learn paths, `MTradeManager::Undo`, `Properties::getProperty`,
`GCTimeLimitItemInfo::getTimeLimit`, `GCNPCAskVariable::getValue`,
`TextBackendSDL`'s font and glyph caches); `PacketIDSet::deletePacketID`, whose
lookup is both a membership test and the iterator it erases - and whose
condition is inverted, a defect left for its own commit; `MTimeItemManager::RemoveTimeItem`,
where `erase(key)` would be the tidy spelling but is not a C++20 helper;
`MSkillDomain::AddSkill`'s linear scan over a map, already rejected by the
first slice; `strstr` membership in `ClientCommunicationManager`, because
`std::string::contains` is C++23; and `platform_config_get_string`'s `strncmp`
prefix in `basic/PlatformSDL.cpp`, which sits behind the same `#ifndef
PLATFORM_WINDOWS` and is not compiled here.

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

**Second span/typed-scalar slice (2026-09-06):** the client skill-activation
family - `CGSkillToSelf`, `CGSkillToObject`, `CGSkillToTile` and
`CGSkillToNamed` - is migrated. It was chosen because it is the largest
remaining cluster of `(char*)&field, szField` casts in `Client/Packet` that
shares one shape (a `SkillType_t`/`CEffectID_t` header, then a target), and
because the golden evidence was almost complete before the work started: the
first three already carry client/server-shared goldens for all six encryption
codes, recorded when task 2.4 pinned the nineteen encrypter users. Only
`CGSkillToNamed` was unpinned - it never reaches the encrypter, so it fell
outside that sweep - and its golden was recorded in a commit of its own, ahead
of the migration, so the bytes it pins are provably the old code's. All four
goldens still pass unchanged, and `tests/wire-layout.txt` is untouched.
`CGSkillToInventory` is the family's fifth member and needed no change: it was
already calling the typed `read`/`write` overloads, which now route through
`readWire`/`writeWire` themselves. `tests/unit/test_packet_skill_family_wire.cpp`
adds what a byte golden cannot see - per-field round-trips through the packets'
own getters under every encryption code, and truncated bodies refused with
`InsufficientDataException`, the same exception the pointer/length read raised,
because both spellings now reach one bounded core.

What stays raw in this family is deliberate. The encrypter branch
(`SHUFFLE_STATEMENT_*` over `readEncrypt`/`writeEncrypt`) is untouched, so the
migration covers the plain branch only and the round-trips run over both.
`CGSkillToObject` stages its `ObjectID_t` through a `std::uint32_t`, as
`CGAttack` does, because `DWORD` is `unsigned long` on this toolchain and so is
not one of the exact-width types the constraint accepts; a `static_assert` now
ties the two widths together so a change to `ObjectID_t` is a compile error
rather than a silent change in how many bytes go on the wire. `CGSkillToNamed`
reads its target name through the `std::string` overload, which is already
bounded by construction, and only its write moved to a span. Reading it
exposed one defect, fixed in its own `fix:` commit on top of the refactor: the
cap was tested after narrowing the length to a `BYTE`, so a 276-character name
narrowed to 20, passed the check, and then wrote all 276 characters behind a
length byte claiming 20. `read()` cannot produce such a name, but
`setTargetName()` accepts any `std::string`; the cap is now applied to the
`std::string` size, before the narrowing, the bounded view ties the emitted
bytes to the length byte just written, and the test file pins it. What the fix
does not do, and every throwing `write()` in the tree shares, is roll back the
framing header `SocketOutputStream` has already put in the ring. That residue
is closed in a `fix:` commit of its own on top of this slice
(`fix/output-stream-header-rollback`, found by this slice's adversarial
review): `SocketOutputStream::write(const Packet*)` now saves the ring's data
length and the sequence byte before the header goes in and restores both when
the body write throws, so a refused packet leaves the stream exactly as it
found it instead of leaving a header the peer would fill from the next packet;
`tests/unit/test_output_stream_rollback.cpp` pins it over both the plain and
the encrypt stream, and the frame on the non-throwing path is byte for byte
what it was.

**Third span/typed-scalar slice (2026-09-10): the packet directories are
free of raw scalar casts.** A count of `(char*)&field, size` reads and
writes over `Client/Packet` found the shape the first two slices were
chosen for almost gone already. `Cpackets` and `Gpackets` hold 1,315 stream
reads and 1,315 stream writes (`iStream.read*(` and `oStream.write*(` with
comments stripped), and every scalar among them but 20 already went through
the typed integer `read(m_Field)`/`write(m_Field)` overloads, which route
through `readWire`/`writeWire` since the first slice. The 20 (26 textually;
six sit in two blocks upstream commented out) were in four packets -
`CGAddZoneToMouse`, `CGBloodDrain`, `GCAttack` and `GCGetDamage`, the
melee combat exchange and the drag-to-cursor pickup - and this slice
migrates them the way `CGSkillToObject` was:
`Coord_t`, `Dir_t` and `WORD` fields go straight to `readWire`/`writeWire`,
and the `ObjectID_t` (a `DWORD`, `unsigned long` on MSVC and so not an
exact-width type there, `uint32_t` off Windows) is staged through a
`std::uint32_t` under a `static_assert` tying the two widths.
`CGAttack`, the first slice's exemplar, staged the same way but without
the assertion; it gains one here, so the six staging sites are uniform.
Only the plain branch of `CGAddZoneToMouse` changes; its
`SHUFFLE_STATEMENT_3` encrypter branch is untouched, and the other three
never reach the encrypter. `Lpackets`, `Upackets` and `Rpackets` had none.
The 132 other two-argument pointer-plus-length calls in the two directories
are a different shape - `char` buffers, `std::string` and `c_str()` reads
such as `m_Name, szName` - already bounded by the span and string overloads
underneath, and not a scalar cast.

The goldens came first, in a commit of their own, so the bytes they pin are
the old code's. `CGAddZoneToMouse` was already under the shared encrypter
set at all six codes. `GCAttack` and `GCGetDamage` are pinned by the server's
`packet_combat_test.cpp` at code 0, so their goldens are byte-identical
copies of its files, with its fixture values, and add nothing to the
cross-repo golden diff (which is not clean: of the 136 files both repos
pin, 134 agree and `CLLogin.code0.hex` and `GCGuildChat.code0.hex` differ,
a pre-existing state this slice does not touch); `CGBloodDrain` has no
server pin and its golden is client-authored. All three are
encrypter-free, and `EncrypterFree()` holds both their write and their
parse code-insensitive.
`tests/unit/test_packet_combat_family_wire.cpp` adds what a golden cannot
see: per-field round-trips under every code, the full 32-bit `ObjectID`
through the staging in both directions (so the cast neither sign-extends nor
narrows), and every truncation of every body, from one byte short down to
empty, refused with the same `InsufficientDataException` the pointer/length
read raised. The field *order* is pinned by the goldens alone - a
symmetric swap of two same-width fields in both `read()` and `write()`
round-trips cleanly and only the golden sees it, which the review
demonstrated by mutation.

What stays raw under `Client/Packet` after this slice is 31 live casts
of the pointer-and-size shape, none of them a packet field (this
paragraph first said 27: it had grepped `(char*)&` and missed the four
the output streams spell `(const char*)&`, the measure-the-spelling
mistake CLAUDE.md warns about). Seven are the framing itself - the packet id,
size and sequence byte that `SocketOutputStream::write(const Packet*)` and
`Datagram::read`/`write(Packet*)` put in front of every body;
`GCMoveOK.framed.code0.hex` pins the three in the stream and **nothing
pins the four in `Datagram`**, which no test frames a packet through.
Sixteen are `Datagram.h`'s own typed scalar overloads, and eight are the
`bool` and `char` overloads of the four socket streams, which do not
route through `readWire`. In the packet directories themselves, `CLLogin` and
`CGConnect` keep four `(char*)m_MacAddress, 6` writes and reads of a
`char` array, the shape a span overload fits and the second slice's commit named as
remaining. Moving the framing belongs to a slice that reads it on its own
terms rather than as one more packet, and that slice should pin the
datagram header first.

**Fourth span/typed-scalar slice (2026-09-10): the framing header, and
with it the whole of `Datagram`.** The slice went as the paragraph above
asked: pin the datagram first, then move. The UDP transport is live -
`CGPortCheck` reaches the login server through it and the `RC*` packets
travel between peers - and nothing had pinned either side of
`Datagram::read`/`write(DatagramPacket*)`. Two goldens now do
(`CGPortCheck.datagram`, `RCPositionInfo.datagram`: the id, the size and
the body), `tests/unit/test_datagram_frame.cpp` builds the frame by hand
and pins the read side through a real `PacketFactoryManager` with its
three refusals (an id at or past `PACKET_MAX`, a size over the factory's
maximum, a length that disagrees with `szPacketHeader + size` either
way), and the two bounded primitives every datagram packet parses through
are pinned at the end of the buffer.

Pinning found two defects, each fixed in a `fix:` commit of its own
between the pin and the move, and both recorded in the code-health
review. `Datagram::write` sizes its buffer at `szPacketHeader + body` and
writes one byte less - the slot the stream's sequence byte occupies, which
both peers count in the length and neither reads - from a buffer a bare
`new char[]` returned, so **every UDP packet the client sent carried one
byte of heap memory**; the server's `Datagram` zero-fills for exactly this
reason and documents the pad as travelling as zero, and the client now
matches it (the test read the pad back as `0xCD` on the unfixed code).
The review's open Medium on the bounds checks - `m_InputOffset + len >
m_Length` wraps, and the write bound was an `Assert` that Release compiles
away - is closed the same way; its read test did not fail on the unfixed
code, it crashed the test process at the first wrapping length.

The move itself: `SocketOutputStream::write(const Packet*)` writes the
id, the size and the sequence byte through `writeWire` (all three are
exact-width types on every platform, so no staging), and `Datagram` gains
what the streams have had since the first slice - `read(std::span<char>)`
and `write(std::span<const char>)` as the one bounded core each way, the
pointer/length pair as adapters, `std::byte` spans beside them, and
`readWire`/`writeWire` under the same `WireScalar` concept, with the
fourteen fixed-width overloads routed through those and `char` read as a
one-byte span with no cast at all. `Datagram.h` was stored with mixed line
endings, the eighteen lines a previous edit had added being LF, and is
CRLF throughout now. Every golden and the wire inventory unchanged; 643
tests, 294,975 checks, 0 failed in both trees after the review's repairs;
`DarkEden` builds with 0
errors, which is the check that every `RC*` packet and `CGPortCheck` still
resolve their `Datagram` calls.

What stays raw under `Client/Packet` after this slice is twelve live
casts of the pointer-and-size shape: the `bool` and `char` overloads of
the four socket streams (eight - `bool` is deliberately unchanged since
the first slice, and `char` could take the one-byte span `Datagram` now
uses; the output pair spells them `(const char*)&`, which is why a grep
for `(char*)&` reports eight in total and not twelve), and the four
`(char*)m_MacAddress, 6` reads and writes in `CLLogin` and `CGConnect`,
which a `std::span<BYTE, 6>` of the array fits. `SocketAPI.cpp` holds six
more at the OS socket calls, which are not wire scalars, and
`CGBloodDrain`'s six sit in comments.

The adversarial review of this slice (two fresh-context readers, one on
the code and one on the claims) found no wire-byte defect and four
things worth recording. The wrap-proof bound as first written, `len >
m_Length - offset`, depended on an offset-never-past-length invariant
that nothing enforces and would *admit* a read if it were broken, where
the old form refused; it is now the review's own recommended form, `len >
m_Length || offset > m_Length - len`, safe whatever the offsets hold, and
it runs before the adapter builds its span, so a hostile length never
becomes an invalid range. The write bound alone still let a body one byte
over its declaration eat the pad slot and go out looking honest, the
peer dropping the last field; `Datagram::write(const DatagramPacket*)`
now holds the body to the size its header declared and refuses either
direction of drift, where the server measures the body and back-fills
the size - the client refuses, the server corrects, and both keep the
byte off the wire. `GLIncomingConnectionError::getPacketSize` declared
one of the two strings its `write` emits, stale against the server's
copy, and now declares both (the client neither sends nor receives it).
And the claims audit found the record overstating in three places: the
write test's wrapping lengths slipped past the old `Assert` too, so it
is a second reproduction rather than a test that "passed only because
the Assert fired"; the Release half of "live in every build" is shown by
no test, the suite being a Debug build; and the residue count above.

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

The second priority-5 slice (2026-09-05) is `Client/MTimeItemManager`, the
register of items that carry a time limit. Its stored deadline was a `DWORD`
holding `timeGetTime()/1000 + lifetime` and is now an absolute point on
`MonotonicClock`'s clock, kept at the whole-second resolution the class has
always counted in. Nothing stayed on the legacy counter: every tick read in
the file was `timeGetTime()`, the deadline is compared only against another
read from the same file and never reaches a packet, another class or
persistent storage, so `MonotonicClock::Now()` is used throughout and
`LegacyTicks()` is not needed here. The public API is unchanged - `AddTimeItem`
still takes a `DWORD` of seconds, because that is a lifetime the server sends
and not a tick, and the four accessors still return `int` fields of a
countdown. Only the `std::map` base's mapped type changed, and nothing outside
the class ever read it.

The quantisation statement for this slice is that there is nothing to state:
these sites read `timeGetTime()`, which `Platform.h` already redefines as
`platform_get_ticks()`, so they were on a 1 ms counter and were never
quantised to `GetTickCount()`'s ~15.6 ms step. The resolution is unchanged;
only the epoch and the width are. The floor to a whole second is preserved
exactly, so an item added part-way through a second still expires up to a
second early and the countdown still steps on the clock's second boundary
rather than on the item's. What the rewrite removes is three defects, which is
why the commit is a `fix:`. The first two are demonstrated in the test beside
the new assertion rather than reproduced against a running server: after 49.7
days of tick the register read every held deadline as up to 49.7 days in the
future instead of long past, and a lifetime near the top of a `DWORD` wrapped
the stored sum and expired the item on arrival - a value the server controls.
The third was the adversarial review's: each old accessor read the clock twice,
and when the second read fell one second after the first the unsigned
subtraction went round, so the description panel could paint 49,710 days on
the very frame an item expired. One read now decides both the sign and the
value, and the remaining count stays a 64-bit `std::chrono::seconds` all the
way to the accessors rather than narrowing to a `DWORD`.
`tests/unit/test_time_item_manager.cpp` drives the wraps through the injected
clock, works the old `DWORD` arithmetic out on the same numbers beside each
assertion, and pins the countdown, the boundary one millisecond before expiry,
the preserved second-floor rounding and the un-narrowed remainder.

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
that function turned up a pre-existing defect, fixed here: the inner
`for(int i = 0; ...)` shadowed the outer `i`, so the
`if(i == m_vs_file_list.size())` after it tested the filter loop's counter
instead, and since the inner loop almost never breaks for a file, every file was
dropped unless `m_filter.size()` happened to equal the list size. The suffix
filter beside it was unbounded twice over: it `strcpy`'d each entry into a
`char[20]` out of the `char[30]` `Start()` fills, and it indexed the name at
`size() - j - 1`, which wraps for a name shorter than the suffix. Both halves
are `basic/FileDialogListing.{h,cpp}` now - moved out of VS_UI so that they
have a test path at all, then fixed test-first
(`docs/RESTRUCTURING.md` task 3.1, `tests/unit/test_file_dialog_listing.cpp`).
The `..`
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
`VS_UI/src/VS_UI_Tutorial.cpp` - and a fourth slice deleted the shims those
walks used to reach. `basic/Platform.h` no longer declares `FindFirstFileA`,
`FindNextFileA`, `FindClose`, `WIN32_FIND_DATA`, `_finddata_t`, `_findfirst`,
`_findnext` or `_findclose`. All of them sat in the `#ifndef PLATFORM_WINDOWS`
compatibility layer, which a Windows build never compiles, and nothing outside
that header named any of them in live code - only comments and the commented-out
tutorial loop. `FILETIME` stayed, because `Client/CrashReport.cpp` declares one,
and so did `INVALID_HANDLE_VALUE`, which `Client/Client.cpp`,
`Client/CGameUpdate.cpp` and `Client/CrashReport.cpp` still compare against.
Priority 6 now has no `_findfirst` or `FindFirstFile` residue left.

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
