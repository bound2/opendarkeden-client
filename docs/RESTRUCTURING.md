# Client Restructuring Plan

**Complete as of 2026-09-09.** Every task below is `done` and has its owner;
PRs #144 and #145 carry the last slices. What the plan leaves behind is the
machinery, not a to-do list: the membership files, the include checker, the
thirteen ratchets and the fix policy (task 3.1) are what keep the end state
true from here, and *What the review rounds settled* is what a later slice
of any kind should read first. The one list still open is 5.2's
*Candidates for a next slice* - dead code the plan found and did not take
unasked - and the exemption list, which shrinks whenever a task extracts a
seam.

Living, trackable plan for moving the OpenDarkEden client's game code out of the
`DarkEden` executable and into testable static libraries, while the outstanding
findings of `docs/code-health-review-2026-08-29.md` keep getting fixed. The end
state: **a fix that touches library code ships with a unit test in the same
commit; code that cannot be tested is a shrinking, named exception rather than
the default.**

This mirrors the server repo's `docs/RESTRUCTURING.md` (same parent directory,
`../server`), which has already executed the exact refactoring this client
needs on its half of the shared codebase: `execute()` stripped off the packet
classes, a dispatch table at the composition root, the whole wire layer
compiled once into a standalone `de-kernel` library, membership held by a file
and enforced by an include-graph checker, and legacy debt tracked by
shrink-only ratchets. Where a task here has a proven server twin, the task
cites it — read the server's status notes before starting, because its
adversarial reviews recorded the traps.

## How to use this document

- Every task has a checkbox and a `> **Status:**` line. Update the status line
  **in the same commit** as the work it describes. Allowed values:
  `not started` | `in progress (<what remains>)` | `done (<commit>)` |
  `dropped (<why>)`.
- A status line records the **current** state and what the next reader
  needs to act: the owner, the conventions the reviews settled, the open
  bugs, what remains. The per-change narrative — what moved, what the
  review round found, the test list, the suite count, the ratchet delta —
  belongs in the PR description and the commit message, not here. PRs #33
  through #77, then #142 to #145, and their commits hold that history for
  everything below.
- A task is only `done` when its **Owner** exists — the test or mechanism that
  keeps the rule true from then on. Landing the change without the owner is
  `in progress (owner missing)`.
- Ratchet numbers only go **down**. Re-measure with the given command before
  and after a change that claims progress; commit the updated number with the
  change. A recorded growth (a split that adds an executable translation
  unit) moves the baseline with the reason written beside it.
- Substantial tasks get an **adversarial review** before merge (two
  reviewers; standing practice in this repo — every round so far has found
  real defects in the change itself).
- Verification for every extraction: both Debug trees build
  (`build/vs2022` with /RTC1, `build/vs2022-asan` with ASan), the test suite
  is green in a plain tree **and** an ASan tree, `wire_inventory_fresh`
  passes, and the user runtime-verifies the client against a live server
  before the branch merges (do not launch `DarkEden.exe` yourself).

### Working agreements for refactoring agents

- **New library targets use explicit source lists, never `file(GLOB)`.** The
  existing globs don't use `CONFIGURE_DEPENDS`, so a moved file silently stays
  in the old target until someone reconfigures; explicit lists make membership
  reviewable and are what the ratchet script parses. After moving files,
  reconfigure both trees.
- **Moving a file between targets must not change its bytes** in the same
  commit when it has no seam to cut. Move first, prove the build, then fix —
  separate commits, so the diff that changes behavior is readable. A file
  that only links once its reaches go through a host moves and cuts in one
  commit (the Phase 4 pattern), and that commit names every change that is
  not the move.
- Sources are CRLF; use the Edit tool, not `sed -i`/`awk`, to modify them.
- Wire layout is pinned: any change under `Client/Packet` that touches
  `read()`/`write()`/`getPacketMaxSize()` must keep
  `tests/wire-layout.txt` in sync (regenerate via
  `tests/tools/gen_wire_inventory.pl`, re-record with `UPDATE_GOLDENS=1`) and
  must be diffed against the server with
  `server/tests/tools/wire_inventory_diff.sh`. A layout change is a protocol
  change and needs the identical change in the server repo.

### What the review rounds settled

Each of these cost at least one review finding; they are the rules the
per-task status lines no longer restate.

- **Measure the value, not the spelling.** A checker that matches
  `array[pPacket->getSlotID()]` misses the hundred sites that copy the
  value into a local first; one that hardcodes a receiver name misses the
  handlers that call it something else. `check_packet_indices.pl` walks
  the value; R7 and R8 exist as a pair because R7 counts a spelling.
- **A ratchet at zero is a claim about the ratchet.** R7 reached 0 and was
  read as finding C19 closed while 24 live sites used an entry copied into
  a static array first. Anything stronger than "the pattern sees nothing"
  needs a different instrument, and a new instrument gets the same
  adversarial reading as the code — it will not get it from the person who
  just built it (R8 was recorded at 13, and the population was 43).
- **A checker pins its own denominator.** `check_format_arity.pl` passed
  three times while checking nothing (a Perl list assignment, `find(1)`
  under PowerShell, a `//` inside a string literal). Both the sites it
  finds and the sites it resolves are floors now; every way a scan can
  shrink must fail rather than report a smaller number and exit 0.
- **A search that finds nothing is a fact about the search.** A grep that
  excludes the name it is searching for (`grep RenderText | grep -v
  TextService`, when every call is `TextService::RenderText`); a per-file
  grep over CP949 sources, which grep classifies as binary and stops
  reading silently; a grep for `#define __USE_ENCRYPTER__` that missed the
  define in `Encrypter.h` while the golden tests already said the branch
  was live. Read what the tests say before declaring code dead.
- **A link proof takes the address of non-virtual members.** A pointer to
  a virtual member is a vtable index and need not reference the defining
  object, so such a proof links cleanly with the file removed from the
  library. Verify a link proof by taking the file back out.
- **Scripted rewrites bound their patterns and are followed by a comment
  sweep.** A non-greedy `.*?` under `/s` swallowed 87 lines of `GameUI.cpp`
  into one argument; the ratchets could not have caught it, because a
  corrupted file has the right number of sites. After a scripted removal,
  sweep for orphaned banners and `//#include` lines.
- **Uninitialised members are reproduced, not guarded.** `/RTC1` fills
  stack locals with `0xCC` and the CRT fills heap with `0xCD`, so a test
  that reads a fresh object fails deterministically; but neither `/RTC1`
  nor ASan flags an uninitialised *member*, so the test reads the fill
  pattern rather than waiting for a crash. Every `gamemodel` slice found
  at least one constructor that set every field but a few.
- **A host entry answers without a host with the value a missing config
  would give, never zero.** A test binary, or a host whose entries are
  NULL, gets the `ClientConfig` constructor's defaults; a missing clock
  means no delay; a missing player means a skipped refresh rather than a
  crash — recorded as a behaviour delta where the old code dereferenced
  unguarded. Host readers are private statics on the class (`MItem::Clock()`),
  guard the function pointer, and are re-read on every call, never cached.
- **A host installer is designated-initialised.** When `WireHost` reached
  24 entries, fourteen shared a signature with another, and the
  executable that installs them is never linked into a test, so a
  positional slip compiled and passed the whole suite; the fourth slice
  recorded the order as "checked by reading". C++20 designators make an
  entry in the wrong slot, or one that does not exist, a compile error.
  What they cannot check is the body an entry points at.
- **A reference on a live line can still be dead.** The whisper seam was
  described as live behaviour because the code that reaches it is
  compiled; every path *to* that code sits behind `0 &&`. Before
  describing what a seam does, find the caller that runs.
- **A class split across a library and the executable** costs nothing when
  the class has no virtuals (`MSkillSet`, `TextService`) or when the
  library constructs none of the split classes (`MBomb`, `MHolyWater`), and
  is otherwise a vtable referencing symbols a test binary cannot link, so
  the class moves whole.
- **Every fix commit names its `Test path:`** (`lib + test`, `moved, then
  fixed`, `exempt`); a fix that could not be reproduced is a *regression
  guard* in its message, not a reproduction. `tools/git-hooks/commit-msg`
  refuses a `fix:` without one.
- **Two ratchets on the same finding see different things.** R3 counts
  `sprintf` lines but `\b` rejects the `w` in `wsprintf`; R7 counts both.
  R4 greps `g_p*` and cannot see a library file calling an executable-side
  *function* (which is what the link proofs are for) or a global under
  another name (`g_Mode`). Read each ratchet's comment for what it cannot
  see before quoting its number.

## Goals / non-goals

**Goals**

1. **Every new fix is unit-tested.** The code a fix touches gets moved into
   (or already lives in) a static library linked by `unit_tests`, and the fix
   is written test-first. Exceptions are named in the exemption list below,
   not decided ad hoc.
2. **The wire layer becomes a standalone library** (`packetwire`): streams,
   encrypter, info classes, and eventually all packet classes — so the #1
   open risk (unvalidated server input in `Client/Packet/Gpackets/`,
   code-health review priority 1) becomes directly testable with hostile
   inputs instead of only greppable.
3. **Game-model logic leaves the executable** piecewise (tables, inventories,
   price/trade logic), cutting `g_p*` global seams as it goes.
4. Architecture rules owned by tests (membership files, include-graph
   checker, ratchets), never by memory.

**Non-goals**

- No behavior change on the wire or in game rules unless a task says so
  explicitly. Layout stays pinned by the wire inventory + the server diff.
- No rewrite of the render/game loop (`GameMain.cpp`, `MZone` drawing,
  `GameUI.cpp`) or the VS_UI widget tree. These stay executable-or-UI-side
  and keep their live-server verification path.
- No new test framework. `tests/framework/test_framework.h` stays; tests are
  files in `tests/unit/` (globbed — reconfigure after adding one).
- Not a port of the server's kernel wholesale: the two repos stay separate
  codebases pinned to one wire contract by the shared inventory.

## Exemption list (tangled, verified at runtime instead)

Code on this list is fixed executable-side with live-server verification and
a **regression guard** note in the commit message, per existing practice.
Shrink it when a task extracts a seam, and record the removal here.

| Code | Why exempt |
|---|---|
| `GameMain.cpp`, `GameInit.cpp`, `Client.cpp`, `SDLMain.cpp` | process lifecycle, DLL whitelist, render loop; also where the hosts (`MItemHost`, `MPriceHost`, `WireHost`) are installed, which no test can prove - `WireHost`'s installer is designated-initialised since 2026-09-09, so a wrong slot there is a compile error, but a wrong *body* still is not |
| `MZone` rendering / `TileRenderer` draw paths | draws through live surfaces; viewer tools cover some of it |
| `VS_UI/src/**` widgets and dialogs | deep two-way coupling with game globals; UI verified visually; `unit_tests` does not link `VS_UI` |
| `Client/PacketHandler/*Handler.cpp` bodies | mutate `g_pZone`/creature state; the *parsers* they consume are in `packetwire` and testable, the mutations are not |
| `PacketFunction.cpp` connect paths | Winsock + connection state machine. `RequestClientPlayerManager.cpp` was listed here until 2026-09-09; task 5.1's fifth slice put its seams behind `WireHost`, and task 5.2's eighth slice deleted it with the rest of the outbound peer side |
| The executable halves of split classes: `MItemUse.cpp`, `MObjectScreen.cpp`, `MSkillAvailable.cpp`, `TextServiceScreen.cpp` | the packet/dialog/drawing side of a class whose core is in a library, by design |

Everything else under `Client/*.cpp` and `Client/Packet/**` is presumed
movable until a task proves otherwise and adds it here with a reason.

## Ratchets (shrink-only)

`tests/ratchet/ratchets.sh` (ctest `ratchets`) holds the baselines inline
and fails the suite when a count **rises** or drops unrecorded, so
tightening lands in the same commit as the progress. Measurements are
grep-only by design; nothing is generated or overwritten. The script's
comment on each ratchet says what it cannot see and keeps the history of
every baseline move; the table below is the current reading.

| # | Metric | Now | What it counts, and does not |
|---|--------|---:|---|
| R1 | Translation units compiled directly into the `DarkEden` target | **484** | `grep -c "<ClCompile Include" build/vs2022/DarkEden.vcxproj`, read from the ctest run's own build dir; SKIP (never PASS) on a generator with no vcxproj, and FAIL on a vcxproj older than the membership files. On the Ninja generator (the Linux and macOS presets) the same count is read from `build.ninja`'s object rules for the target against its own baseline, **481** - the non-Windows source list is three files shorter - so the ratchet no longer skips there (2026-09-08). Baseline 1,044 on 2026-09-01. 489 → 484 on 2026-09-09: the last holdout moved into `packetwire`, then `WhisperManager.cpp` and three `RC*` handlers went with the outbound peer side (task 5.2's seventh and eighth slices); this row was not moved with the first two of those and the script's own FAIL message is what says to. It counts what still cannot be unit-tested. Recorded growths, each the executable side of a split: `PacketHandlerRegistry.cpp`, `GCExchangeBuyHandler.cpp`, `MItemUse.cpp`, `MObjectScreen.cpp`, `MSkillAvailable.cpp`, `TextServiceScreen.cpp`. |
| R2 | Packet `.cpp` files still defining a packet-style `::execute(Player` | **0** | `grep -rlE '^void\s+\w+::execute\s*\(\s*Player' Client/Packet/{Gpackets,Cpackets,Lpackets,Rpackets,Upackets} --include='*.cpp' \| grep -v Handler \| wc -l`. Baseline 448. Holds the line since `Packet::execute` itself was deleted; the client twin of the server's R4. |
| R3 | Live `sprintf`/`strcpy`/`strcat` lines under `Client/Packet` and `Client/PacketHandler` | **0** | Line-based; strips `//` tails before matching, so a commented-out call does not count. `\b` rejects the `w` in `wsprintf`, which R7 sees instead. Baseline 61 (a quarter of it commented-out code). Holds the line since the packet-tree copy pass (2026-09-04, PR #76). |
| R4 | Library-compiled `.cpp` files referencing `g_p*` client globals no library file defines | **21** | Over the library dirs (minus CMake-excluded files) plus the `packetwire` and `gamemodel` membership files; comment lines excluded; the subtraction is library-wide, so a library file reading a global another library defines is not a seam. **All 21 are `VS_UI` files.** Blind to a library file calling an executable-side *function* (the link proofs cover that) and to a global not named `g_p*`. Baseline 83. |
| R5 | Direct packet `execute()` call sites outside `Client/Packet` | **1** | A commented-out block in `CGameUpdate.cpp`. Added when the 2.2 review found the client fabricates packets locally and calls `execute()` on them; a live caller is a compile error now, before it is a ratchet failure. |
| R6 | *retired* — `packetwire` members calling `SendBugReport` | — | Lived one slice (2026-09-03). Added to replace the failed-link detector that stubbing the symbol had disabled; fired on the next promotion (count 2), which said "move the function, not the seam". `SendBugReport` is in `Client/Packet/WireHost.cpp`, the stub is gone, and the link is the detector again — narrower, since a link catches a call only in a library `unit_tests` links and in an object a test pulls in, which is what the address-taking link proofs in `test_wire_host.cpp` and `test_player_base.cpp` guarantee. |
| R7 | Call sites handing a game string table entry to a printf as its **format**, where the lookup is spelled at the call site | **0** | Five alternatives: the `sprintf` family (`fprintf` included), the size-taking family, `AddFormat`, the offset-append form `sprintf(buf + strlen(buf), …)`, and `.Format` (`MString::Format` is a printf reached as a method). The tree is joined before matching because sites put destination and format on different lines. **Blind to indirection**: an entry copied into a static array or a local first is invisible. Baseline 293; holds the line since PR #71. On its own it is not a measure of finding C19. |
| R8 | printf-family calls whose **format argument is not a string literal**, across `Client`, `VS_UI` and `basic`, headers included | **43** | The population R7 measures a spelling of; it cannot tell a table entry from a legitimate forward, so renaming cannot satisfy it. The 43 were read: 28 vararg forwarders, 6 inside `SafeFormat`'s `Emit`, 3 literals behind `TEXT()`/`_T()`, 6 declarations. The family list was enumerated from the tree (`fprintf`, `vswprintf` included). Cannot see a destination containing parentheses; those 16 sites were audited by hand, all literal formats. Added 2026-09-04 (PR #73). |
| R9 | Dynamic exception specifications (`throw()`, `throw(X, Y)`) under `basic/` | **0** | Not a removal: the first conformance slice went looking and found `basic/` had never carried one. The parentheses must hold type names or nothing, which is what tells a specification from a `throw` statement - the six live `throw ("...")` statements in the shop packets do not count, and neither does `throw Error(...)`. Blind to a type list it cannot spell (a template argument, a pointer, a comment between the parens), to a specification inside a `/* */` block, and to one behind a macro. The file list is asserted non-empty, so a renamed `basic/` fails instead of measuring 0 of nothing. Added 2026-09-06. |
| R10 | The same, across the library set: `basic`, `Client/SpriteLib`, `Client/TextSystem`, `Client/DXLib`, `Client/framelib` and `VS_UI` as whole trees, the `gamemodel` membership file, the `packetwire` membership file **and every `.h` under `Client/Packet`** | **0** | All of it is the packet tree - every other library is already at 0. 2,211 in the membership file's `.cpp` and 9,252 in the headers, over 1,042 of the set's 1,473 files; 8,528 empty `throw()` and 2,971 type lists. The headers are in the set because a specification is part of the function type, so a `.cpp`-only metric would have counted half of every edit as progress. Files are joined before matching: five specifications in `SocketAPI.cpp` span two lines, and line-based this set reads 11,494. Outside it, and the rest of the workload: `Client/PacketHandler` (284), the remaining executable sources (49) and `tests/` (9). Added 2026-09-06. **9,664 on 2026-09-07:** the 162 files directly under `Client/Packet` are at 0 - 143 carried a specification - and so is `tests/` (1,799 + 9 sites: the wire core, the packet framework and players, the info classes); ten destructors that carried a type list are spelled `noexcept(false)`, which the pattern does not count, and the remainder is the packet directories. **9,055 on 2026-09-07:** Lpackets, Upackets and Rpackets at 0 as well (609 sites, by script), leaving Cpackets and Gpackets. **5,883 on 2026-09-07:** Cpackets at 0 (3,172 sites, by script; the one packet that derives from another keeps its base unspecified), leaving Gpackets. **0 on 2026-09-07:** Gpackets at 0 (5,883 sites, by script; two packets deriving from GCChangeInventoryItemNum keep its getPacketSize unspecified). The library set is clean; what R10 never covered - `Client/PacketHandler` (284) and the executable sources (34) - is the next slice, with a ratchet of its own. |
| R11 | `register` storage-class specifiers in every `.cpp`, `.h` and `.inl` under `Client`, `VS_UI`, `basic`, `tools`, `third_party` and `tests` | **0** | Closed finding 4 of the assessment: 627 in 23 files removed 2026-09-07, all mechanical. Counted by `tests/tools/count_register.pl`, a tokenizer that strips comments and string literals per file, because the R9/R10 pipeline strips `/* */` before `//` and reads the blitters' `//*pDest = ...` line comments as block-comment openers (over the 23 files on master it saw 538 of the 627, a figure that moves with file order) and would count the NPC script strings that say "register as a couple". The `.c` files stay outside: they compile as C, where the keyword is valid. Added 2026-09-07. |
| R12 | Dynamic exception specifications anywhere: every `.h`, `.cpp` and `.inl` under `Client`, `VS_UI`, `basic`, `tools`, `third_party` and `tests` | **0** | R9 and R10 cover the libraries; this holds the executable side too, with the same pattern and blind spots. The last 319 outside the library set - 284 in `Client/PacketHandler` (one per handler `execute` definition), 32 in the two request-side packet factory managers, three in `RequestFileManager` and `Updater/UpdateManager.h` - went 2026-09-07, all type lists or `throw()` deleted, none promoted. Finding 3 of the assessment is closed on the source side. Added 2026-09-07. |
| R13 | Platform macros spelled outside `basic/Platform.h`: `__LINUX__`/`_LINUX` anywhere in the C++ tree, `PLATFORM_MACOS` outside `basic/`, and `PLATFORM_MACOS` in any CMake file | **1** | The port's build-contract slice (`docs/linux-macos-port-assessment-2026-09-07.md`, area A). Before it, four spellings of Linux coexisted and the build defined none - `__LINUX__` at 63 sites, so the socket and file APIs compiled neither their Windows nor their POSIX branch off Windows - while CMake handed every non-Windows target `PLATFORM_MACOS` in six places. `Platform.h` now detects the platform alone and adds `PLATFORM_POSIX`; the POSIX branches test that, and CMake passes no platform macro off Windows. `basic/` is exempt from the macOS count because `PlatformSDL.cpp`'s mach-o code is genuinely Darwin-only. The one counted use is the macOS font list in `TextBackendSDL.cpp`, a real Darwin branch; a second is a deliberate baseline change. Blind to the compiler builtins (`_WIN32`, `__APPLE__`, `__linux__`). Counted by `tests/tools/count_identifier.pl`, the register tokenizer generalised. Added 2026-09-08. |
| R14 | Live `GetTickCount()`/`timeGetTime()` calls under `Client` and `VS_UI` | **113** | The clocks work of the C++20 assessment (priority 5): the 32-bit tick wraps every 49.7 days, and every `previous + delay <= now` over it fires early or stalls across the wrap. Counted by `tests/tools/count_tick_reads.pl`, a character-level scanner over comments and string literals, because the regex strip the clocks slices first counted with reads a `//*` line comment as a block opener and swallows live code up to the next `*/` - the failure R11's tool was written for - and so missed the `srand(GetTickCount())` in `VS_UI_Item.cpp` outright (209 in 32 files where this reads 214 in 34). `basic/` is outside the count: its three hits are `Platform.h`'s definitions. Counts calls and definitions alike, so the 186 includes the non-Windows `GetTickCount` shim in `VS_UI_widget.h`, and counts `#if`-disabled code. 214 before the widget timers outside `GameCommon` moved to `MonotonicClock::IntervalTimer`; 100 of the 186 were the two `VS_UI_GameCommon` sources. Added 2026-09-10. **138 later that day:** the interval and window gates in those two sources moved (45 calls), the last `GetTickCount` in `VS_UI` with them - two `srand` seeds reseeded and the `VS_UI_widget.h` stub definition deleted make the 48; `VS_UI` holds 56 `timeGetTime()` sites in three files (the header's `SetTimer` is the third), four of them `srand` seeds and the rest deadline and elapsed-time shapes; `Client` 82. **113 later still:** the deadlines set and read within `VS_UI` and the flag-war end its handler sets moved to `TimePoint` (25 calls); the two mission structs `VS_UI` had redeclared from the wire layer became typedefs of it; `VS_UI` holds 33 (the minigames' clocks and the three deadlines the executable sets through shared structs), `Client` 80. |

R7 and R8 exist as a pair on purpose: R7 is precise and blind to
indirection, R8 is coarse and cannot be evaded by spelling. Finding C19's
closure rests on both plus the hand audits listed in the review's C19 entry,
never on either number alone.

**Two checkers sit beside the ratchets**, in `tests/tools/`, because what
they measure needs parsing rather than grepping. `check_format_arity.pl`
(ctest `format_arity`) compares every converted format site's arguments
against the built-in English table in `MGameStringTable.cpp` (which
`InitGameStringTable()` installs over the file data on the English path, so
it is what the default build formats with); it fails when an entry asks for
more arguments than the site passes or a conversion's argument is provably
the other kind, and it floors both the sites it finds (301) and the sites it
resolves (289). `check_packet_indices.pl` (ctest `packet_indices`) is the index
half of code-health priority 1: over `Client/Packet` and
`Client/PacketHandler` it walks packet-derived values into subscripts —
through locals, across lines, one hop — and reports **114**, 101 into a
named, verified `CTypeTable` and a ceiling of **13** into a container that
is not, all guarded today. Its range-checked list is a named allowlist that
fails closed (`CMessageArray::operator[]` truncates rather than checks, so a
pattern over `(*g_p...)` would have excluded it wrongly); a fourteenth raw
subscript has to be read before the number moves.

---

## Phase 0 — Scaffolding

- [x] **0.1 This document.**
  > **Status:** done (2026-09-01).
  - Owner: the status-line discipline itself; CLAUDE.md points here.

- [x] **0.2 Ratchet script.** `tests/ratchet/ratchets.sh`, registered in
  `tests/CMakeLists.txt` as a ctest, ports the server's script shape: fails
  on increase and on unrecorded decrease; generates into scratch space, never
  overwrites tracked files in place.
  > **Status:** done (2026-09-01, PR #36). `.gitattributes` carries
  > `eol=lf` for committed test data and scripts (the server's lesson).
  > `check()` fails on an unmeasurable value instead of passing it, and
  > every ratchet asserts the directories it greps exist, so a rename
  > cannot fail-open at a baseline of 0.
  - Owner: the ratchet test.

- [x] **0.3 Include-graph checker.** `tests/arch/check_includes.pl` (perl —
  Git for Windows ships it), run by ctest as `arch_includes`.
  > **Status:** done (2026-09-01, PR #36; rules extended per phase).
  > Current rules: **W0** every `.cpp` under `Client/Packet` is in exactly
  > one of `tests/arch/packetwire_files.txt` and
  > `packetwire_holdouts.txt` (keys case-folded; an indented membership
  > line is refused, because CMake anchors at column 1); **W1** a
  > `packetwire` member's include closure stays inside `packetwire`,
  > `basic/` and system headers; **W2** no `MinTr.h`/`DebugInfo.h`/
  > `DebugKit.h` in that closure; **M0–M2** the same for
  > `gamemodel_files.txt` (every listed file exists under `Client/`; the
  > closure may include only `basic/`, `Client/framelib/`,
  > `Client/Packet/`, `Client_PCH.h` and the file's own `.h` lines).
  > Angle includes that resolve inside the tree are checked like quoted
  > ones (the library's include path carries `Client/`); the search order
  > mirrors `target_include_directories(packetwire)`; `#if`/`#ifdef` on
  > the one-meaning macros (`__GAME_CLIENT__` defined, server macros
  > never) are evaluated so dead server headers are skipped as the
  > compiler skips them; an unresolvable include is a violation; the walk
  > dies on an empty file list. `tests/arch/baseline.txt` is empty by
  > design.
  - Owner: the `arch_includes` ctest.

---

## Phase 1 — `packetwire`: the wire-support library (pilot)

The first extraction, chosen so that no source file needed to change to
move (see *First candidate* at the end) and the result immediately covered
the top-risk area's foundations: streams, framing, crypto, sockets and the
info classes the GC packets delegate their parsing to.

- [x] **1.1 Create the `packetwire` static library** from the game-free
  subset of the `Client/Packet` root.
  > **Status:** done (2026-09-01, PR #34; 51 files then, the whole wire
  > layer bar one file now — see 2.4 and 5.1). Membership is
  > `tests/arch/packetwire_files.txt`, read by CMake, the include checker
  > and the ratchet script; the executable's list drops members by
  > absolute path. Build wiring: `__GAME_CLIENT__` from `Client_PCH.h`,
  > `__WIN32__`/`__WINDOWS__` on WIN32, include dirs `Client/Packet`,
  > `Client`, `basic`; `DarkEden` and `VS_UI` link it.
  - Owner: the membership file + W0/W1/W2 in the include checker + R1.

- [x] **1.2 Link `packetwire` into `unit_tests`** and land the first parser
  tests. Stream construction in tests goes through a one-line
  `friend class SocketInputStreamTestAccess;` in the stream header
  (access-only, declared unconditionally so the class definition is
  identical in every TU); the helper is `tests/support/packet_stream_access.h`.
  Test TUs compile with the library's own defines (`__GAME_CLIENT__=1`,
  Windows wire macros): `Packet`'s virtual set changes under them, so a
  mismatch is a real vtable/ODR break.
  > **Status:** done (2026-09-01, PR #34). Pinned: stream bounds
  > (zero-length rejected, over-read throws `InsufficientDataException`
  > and consumes nothing, wrap-around reassembly), the `read(std::string&,
  > len)` contract including the truncate-at-embedded-NUL-but-consume-
  > full-length asymmetry, `ModifyInfo` and `InventoryInfo` hostile
  > counts. The five allocate → read → push_back parser sites
  > (`InventoryInfo`, `GearInfo`, `ExtraInfo`, `RideMotorcycleInfo`,
  > `PCItemInfo`) push_back before read so a truncated payload does not
  > leak the in-flight slot — a regression guard, because MSVC's ASan
  > does no leak detection.
  - Owner: the tests themselves; `unit_tests` link line.

- [x] **1.3 First test-first fix: `StringStream` stack overflow.**
  `operator<<(float)` did `sprintf(buf, "%f", T)` into `char buf[12]`;
  `double` the same into `buf[22]`. The server fixed this family on its
  side (server RESTRUCTURING 1.4).
  > **Status:** done (2026-09-01, PR #34). The nine-operator numeric
  > family uses `snprintf` with range-sized buffers (`double` 352 — `%f`
  > of `-DBL_MAX` is 317 characters), and `m_Size` is `size_t` (it was
  > `ushort` and wrapped at 64 KiB, which the widened entries made
  > reachable). Tests pin the exact formatting at each old overflow
  > threshold.
  - Owner: the unit test.

---

## Phase 2 — Strip `execute()`, move the packet classes

The client twin of the server's tasks 2.3/2.4: the migration recipe, the
dispatcher design and the traps are recorded in the server's status notes.
This is what makes the `Gpackets` parsers (priority 1 in the code-health
review) directly unit-testable.

- [x] **2.1 `PacketDispatcher` in `packetwire`.** Table of packet id →
  `void(*)(Packet*, Player*)`, written only at startup; the server's
  `PacketDispatcher.h` shape verbatim, `DE_REGISTER_PACKET_HANDLER` macros
  included.
  > **Status:** done (2026-09-01, PR #37). Fixed `PACKET_MAX` table,
  > unconditional throws on double registration and out-of-range ids,
  > `InvalidProtocolException` from `dispatch` on an unregistered id.
  > All four receive loops (`ClientPlayer`, `Player`,
  > `RequestServerPlayer` - `RequestClientPlayer` was a fifth until task
  > 5.2's eighth slice - and `ClientCommunicationManager` with a NULL
  > player) call
  > `PacketDispatcher::dispatch` unconditionally; the transitional
  > `tryDispatch` fallback was deleted with `Packet::execute` itself in
  > 2.4, so the base class carries no handler entry point, as on the
  > server.
  - Owner: R2 ratchet + the dispatcher unit tests.

- [x] **2.2 Migrate the GC direction** and
- [x] **2.3 Migrate CG / LC / CR-RC / U.**
  > **Status:** done (2026-09-01, PRs #38 and #39). Every `execute()` body
  > in every direction was mechanically classified and stripped.
  > `Client/PacketHandlerRegistry.cpp` is the composition root, called
  > from `InitSocket()` (per login attempt), idempotent, its flag set
  > after success: the standard delegations, the packet-only handlers
  > (the datagram connection family), the `__BEGIN_DEBUG` thunks (kept
  > for the cout-branch platforms), `GLIncomingConnectionError`'s cout
  > trace, `GCExchangeList` and `CGConnectSetKey` as explicit no-ops.
  > The `DE_REGISTER` thunks carry `__BEGIN_TRY`/`__END_CATCH`, so the
  > per-packet stack-annotation frame the deleted bodies had is kept.
  > The 163 CG `execute()` bodies were deleted unregistered, with
  > `CGHandlersStub.cpp`; the honest basis, written in the registry
  > header: the server never sends CG/CL ids and the bodies were no-ops
  > (`PacketValidator`'s `CPS_NORMAL` accepts any id and
  > `Player::processCommand` has no validator, so the only change is
  > that a protocol-violating peer now disconnects — the server's trade).
  > **The receive loops were never the whole story**: the client
  > fabricates packets locally (skill echoes in `CGameUpdate.cpp`, GM
  > messages in `Client.cpp`, `CGConnectSetKey` on the login and
  > reconnect paths); all route through `PacketDispatcher::dispatch`
  > and R5 owns the rule.
  - Owner: R2 and R5 ratchets.

- [x] **2.4 Move the packet classes into `packetwire`**, plus
  `PacketFactoryManager` / `PacketValidator` / `PacketIDSet`; handlers move
  to `Client/PacketHandler/`, compiled into the exe, so `Client/Packet` is
  wire-only and the include checker locks the whole directory.
  > **Status:** done (2026-09-01, PR #41). Handlers moved by pure `git
  > mv` after one prep commit qualified their includes
  > (`"Gpackets/GCSay.h"`); the never-compiled CG handlers were deleted.
  > The last game reaches came out by seam: `CLLogin`'s
  > `g_pUserInformation` read is a packet member the sender sets,
  > `WHISPER_MESSAGE` sits beside `CRWhisper`, five root info classes
  > moved under `Client/Packet`, two dead `__GAME_SERVER__` bodies that
  > built wire fields from live game objects were deleted.
  > `PacketDiagnostics` is the hook through which `Datagram::read`
  > reports without linking the executable; since 5.1 it is an
  > interception point (how a test captures the text), and reporting goes
  > straight to the library's `SendBugReport` when no hook is installed.
  > **Goldens (the owner):** `test_packet_goldens.cpp` writes through the
  > real streams and pins the `.hex` files under `tests/golden/`, **54 of
  > them byte-identical copies of the server's** (all 19 encrypter packets at codes 0–5, GCMoveOK
  > framed, CGSay, CGWhisper) — `diff -r` of the two golden directories
  > is the cross-repo check. The first run found a real wire defect:
  > `CGMove` at encrypt code 0 wrote x,y,dir where the server reads
  > dir,x,y, and code 0 is reachable (the session code cancels for zone
  > 1301 in the shipped data); fixed on the client, the reading side
  > being the authority. Two asymmetries are pinned as fact-tests:
  > `CLLogin::read` here is one byte short of the login server's, and
  > `GCDropItemToZone` round-trips here while the server pins only its
  > `write()`. `test_packet_factories.cpp` proves the link for the
  > received directions and pins that the manager refuses CG and
  > out-of-range ids.
  > **Deliberately not done:** the ~186 `__GAME_SERVER__`/`__GAME_CLIENT__`
  > conditionals in 157 packet sources (they have one meaning in every
  > target, so the checker evaluates them; sweeping the dead server
  > halves is 5.2 work), and goldens for the ~500 unpinned packets.
  - Owner: W0/W1 over the whole `Client/Packet` tree; the goldens; the
    factory link test.

- [x] **2.5 Retire the wire-inventory workaround.** `test_wire_layout.cpp`
  calls the real factories instead of perl-lifted method bodies;
  `tests/wire-layout.txt` stays byte-compatible with the server's
  `wire_inventory_diff.sh`.
  > **Status:** done (2026-09-02, PR #42). `tests/generated/WireInventory.inc`
  > has the server's registry shape (one include and one registration per
  > factory class; `gen_wire_inventory.pl` emits it, `wire_inventory_fresh`
  > pins it, and the generator refuses a `getPacketName()` spelling it
  > cannot check). The test constructs every factory and its packet (the
  > link proof for the written CG/CL directions), checks id uniqueness,
  > and checks every id `PacketFactoryManager::init()` serves is a listed
  > factory at its own max size. Rpackets are constructed but kept out of
  > the rendered file. Two divergences went first: 112 factory classes
  > had been compiled out of every client build (`__DEBUG_OUTPUT__`,
  > `__GAME_CLIENT__` guards) and are unconditional now, and `CRRequest2`,
  > a dead duplicate of `PACKET_CR_REQUEST`, is deleted. Two wire defects
  > found and fixed test-first: `GCUpdateInfo`'s constructor never
  > initialised `m_pBloodBibleSign` while its destructor deletes it (a
  > truncated body freed a garbage pointer); and `GCAddItemToItemVerify`
  > read two parameters for `UP_GRADE_OK` where the server writes one, so
  > every successful item-grade upgrade over-read four bytes into the next
  > packet (its `write()` also dropped both `THREE_ENCHANT_OK` parameters;
  > the handler now sets the grade the server sends). The other seven
  > packets a cross-repo sweep had flagged are identical layouts.
  - Owner: `wire_inventory_fresh`; the all-factory construction in
    `test_wire_layout.cpp`; the `GCAddItemToItemVerify` goldens.

**Phase exit criteria:** met — R2 = 0; `Client/Packet` contains no handler
code; parser fixes are written test-first against real packet objects;
live-server smoke tests passed per slice.

---

## Phase 3 — The fix policy (in force from Phase 1 onward)

Not a code phase — the standing rule this plan exists to enable, stated once:

- [x] **3.1 Every fix names its test path in the commit message.** One of:
  (a) *lib + test* — the code is in a static library and the fix commit
  contains the test; (b) *moved, then fixed* — the fix's first commit moves
  the unit into a library, the second fixes it test-first; (c) *exempt* —
  the code is on the exemption list, the commit says so and carries the
  regression-guard wording. A fix commit that is none of the three is wrong.
  > **Status:** adopted 2026-09-02; enforced since PR #53 by
  > `tools/git-hooks/commit-msg`, which refuses a `fix:` commit without a
  > `Test path:` line naming one of the three (no bypass flag on purpose;
  > mode 100755, pattern anchored). Installed per clone with
  > `git config core.hooksPath tools/git-hooks` — CLAUDE.md carries the
  > instruction. Review-round repair commits are not exempt.
  - Owner: the hook.

---

## Phase 4 — Game-model extraction

Background work, one class family per branch, each independently mergeable.
Target library: `gamemodel` — links `basic` + `packetwire` + `framelib`
(+ iconv for `MString`), **no** SDL/dxlib/VS_UI. Membership is
`tests/arch/gamemodel_files.txt` (`.cpp` lines compiled, `.h` lines
allowed in the closure), read by CMake, which removes each member from the
executable by absolute path and asserts it, by the include checker (M0–M2)
and by the ratchet script (R4). Extraction means adding a file to that list
and cutting its `g_p*`/UI seams through a host struct the executable
installs at start-up (`MItemHost`, `MPriceHost`; see *What the review
rounds settled* for the host rules). Test fixtures share
`tests/support/gamemodel_world.h`.

- [x] **4.0 Compile the VS_UI client sources once.** The 36 `Client/*.cpp`
  files on `VS_UI_CLIENT_SOURCES` were compiled into both `VS_UI` and the
  executable (the list was relative, the exe's `REMOVE_ITEM` absolute, so
  it never matched — the LNK4217 noise CLAUDE.md used to describe).
  > **Status:** done (2026-09-02, PRs #46 and #47). The list is gone and
  > the files compile once, into the executable, which is the side that
  > linked all along: a linker map of `DarkEden.exe` before and after
  > (989,469 symbol → object rows, 0 differ) is the proof. `_LIB` is
  > `PUBLIC` on the `VS_UI` target: its ~100 `#ifndef _LIB` regions are
  > the standalone UI test harness, some of them class members, so the
  > 72 executable translation units that include VS_UI headers must see
  > the same layout the library's objects do. Unverified, not knowingly
  > broken: on `NOT WIN32` `Client/Client.cpp` stays in `VS_UI` (the
  > non-Windows executable filters out its WinMain).
  - Owner: the linker-map comparison (in PR #46); R4 no longer parses a
    CMake list.

- [x] **4.1 Pure tables first:** `ExperienceTable`, `MItemOptionTable`,
  `MGameStringTable`, `MSoundTable`, `SystemAvailabilities`, `FameInfo`.
  > **Status:** done (2026-09-02, PR #44). `gamemodel` exists with the
  > six tables, `ExpInfo`, `MString`, `MStringArray`. Seams cut:
  > `UseEnglishText` takes the `Properties` table (`UseEnglishTextFrom`
  > underneath is the testable core), and
  > `SystemAvailabilitiesManager::LoadFromStream(std::istream&)` takes
  > the lines `GameInit.cpp` reads out of the archive — the shape every
  > later file loader took. Fixed test-first: `ITEMOPTION_TABLE::LoadFromFile`
  > wrote part names past two fixed `MAX_PART` arrays for whatever count
  > the file declared; `ITEMTABLE_INFO`'s constructor left `Price`,
  > `Race`, `DropFrameID` unset. **Known, not fixed:** a refused
  > item-option table leaves the part-name `MString`s NULL and three
  > `VS_UI` call sites `strcpy` them unguarded (pre-existing for any
  > unset part; the alternative was heap corruption at startup).
  - Owner: the membership file, the CMake assertion, the include checker.

- [x] **4.2 Money/price/trade logic:** `MMoneyManager`, `MTradeManager`
  (with `MSortedItemManager`), `MPriceManager`.
  > **Status:** done (PRs #45, #57, #58). The money manager's one reach —
  > the storage-box help hint past 100,000 — is a per-wallet hook the
  > executable installs on the player's wallet (the trade and storage
  > wallets carry none); `operator=` keeps the target's hook. The trade
  > manager's accept delay reads the clock `MItemHost` carries. The price
  > manager goes through **`MPriceHost`** (race, level, stat sums, the
  > potion and gamble half-price events, the shop tax percentage carried
  > unsigned as the server sends it); without a host a price carries no
  > player, event or skill adjustment. Fixed test-first: `CanAddMoney`
  > ignored the balance, so a wallet near the limit said yes and the
  > `AddMoney` after it said no, with the other side's money nowhere to
  > go; `MItem`'s constructor never set `m_bTrade`, the grid position or
  > the durability, so an item arriving during a trade could be deleted
  > by `Trade`; the gamble price's tax multiply overflowed a 32-bit `int`
  > above 21,474,836 (64-bit on both paths now). Executable-side, exempt:
  > `MPetItem` never set its remaining experience or food type.
  > **Known, not fixed:** `CancelTrade` refunds money only — the offered
  > items keep their flag until the next trade start clears it, and a
  > refused refund still answers true; `bMysterious` is a dead parameter
  > `UIMessageManager` still computes; a star price for item type 0 is
  > −20 stars.
  - Owner: the membership file, the CMake assertion, the include checker.

- [x] **4.3 Containers:** `MItemManager`, `MGridItemManager`,
  `MSlotItemManager`, `MQuickSlot`, `MInventory`, `MStorage`, `MShopShelf`.
  > **Status:** done (PRs #54 and #56). The containers' two reaches — the
  > player's affect check on an item and the inventory sound it makes —
  > are `MItemHost::RefreshAffect` and `PlayItemSound`, host-guarded
  > statics on `MItem` so no caller checks for a host; a NULL player is a
  > skipped refresh where the old code would have crashed (recorded).
  > `MCorpse` stays executable-side (it owns an `MCreature`). Fixed
  > test-first: the slot manager wrote the item into its slot before the
  > id map could refuse it, and the grid's `ReplaceItem` removed the
  > occupant and returned true when the map refused the newcomer — in
  > both, the belt/stash/store handlers then `delete`d the refused item,
  > leaving the container holding a freed pointer; and
  > `MShopShelf::NewShelf` indexed its three-entry factory table with the
  > shelf type straight off the wire (`GCShopList`), calling through a
  > code pointer past the table for a value of 3 or more — the factory
  > answers NULL now and both handlers return on it (`SHOP_RACK_SPECIAL`
  > and `SHELF_SPECIAL` are both 1, said at the call).
  - Owner: the membership file, the CMake assertion, the include checker;
    `test_item_containers.cpp`, `test_inventory_storage_shop.cpp`.

- [x] **4.4 Item/skill cores:** `MItemTable`, `MItem`, `MObject`,
  `UserInformation`, `ClientConfig`, `MTimeItemManager`, the gear
  (`MPlayerGear` and the three race gears), `MShop`, `MSkillManager`,
  `MSkillInfoTable`, `SkillDef`.
  > **Status:** done (PRs #48, #53, #59, #60, #61). Everything the task
  > lists is in the library but the halves that are the packet and UI
  > side of items and skills, executable by design: **`MItemUse.cpp`**
  > (every class with a `UseInventory`/`UseQuickItem`/`UseGear` body,
  > moved whole, with the factory table), **`MObjectScreen.cpp`** (the
  > two screen-rectangle members that read draw interpolation state) and
  > **`MSkillAvailable.cpp`** (`SetAvailableSkills`, its vampire
  > counterpart and `CheckMP` — what the player can use *right now* —
  > plus the two war-bonus arrays). **`MItemHost`** carries the animation
  > clock (also the skill-use and trade delays), the top view's item-drop
  > frame pack, `RefreshAffect`, `PlayItemSound`, `RecalculateStatus`,
  > `ResetQuickItemSlot`, `RepairHint` and `EmptyMagazineFor` (the
  > magazine-fitting loop lives in `GameInit`); `GameInit.cpp`'s
  > `InitSkillTree` feeds `LoadFromFileServerDomainInfo` a stream. The
  > `__GAME_CLIENT__` guards in the moved files (always on) went with the
  > includes they wrapped.
  > **Fixed test-first, in the library:** `IsQuestItem` tested the item's
  > own flag only when the timed-item register existed; the requirement
  > getters returned `BYTE` while the slayer ceiling is 295 (a level-150
  > item looked easy to equip); `ITEMOPTION_INFO`, `SKILLINFO_NODE`
  > (`m_SkillStep`, which `AddSkill` branches on) and the item table rows
  > had constructors that left fields unset; `CheckItemStatus` compared an
  > unsigned percentage against `int` thresholds read unchecked from the
  > configuration, so a negative threshold graded every worn piece as
  > almost broken; the zap branch of `AddItem` in all three gears read the
  > wrong slot and accepted a ring-less zap whenever that slot was full;
  > `RemoveItem(GEAR_*)` indexed the slot array with an unbounded id from
  > `GCRemoveFromGear`; `MShop::SetShelf` wrote `m_pShelf[n]` unbounded;
  > `LoadFromFileServerDomainInfo` indexed its eight-row table with an
  > `int` off the file through the raw pointer; `MSkillDomain::Clear`
  > could never reset its level counters (its `!=NULL` branch sat after
  > the pointer was nulled), so a reload left a domain whose learned-level
  > array was NULL and the next `UnLearnSkill` indexed it —
  > `SetStateFromSkillList` now derives the step lists and the array from
  > the skill list, `ClearSkillStep` clears the step lists a rebuild used
  > to append to, the loader checks its reads and a refused experience row
  > costs only itself; `LearnSkill` wrote `m_pLearnedSkillID[level]` with
  > the level straight from the skill file into an array sized by the tree
  > walk (refused now, before the skill enters the usable set).
  > **Known, not fixed:** the Ousters gear plays the gear sound and
  > recomputes the stats twice per item that goes on through its slot
  > table; `MSkillDomain::SaveToFile`/`LoadFromFile` have no caller
  > anywhere (reachable only through `CTypeTable`'s own file I/O; deleting
  > them is 5.2); `SKILLDOMAIN` (`SkillDef.h`) and `SkillDomain`
  > (`Packet/Types/CreatureTypes.h`) are the same eight names and
  > `MSkillManager.cpp` indexes itself with both; `LearnSkill`'s
  > "next level" gate is commented out, so any level can be learned in
  > any order; `IsPassive()` and `GetNextSkillList()` have always read
  > defaults in the client (their setters' only callers were the deleted
  > server data); the gear and shop sources hold ~490 Korean comment lines
  > and their headers ~180 (translating them wholesale would swamp the
  > byte-identity the move rests on). Untested library code:
  > `MSlayerGear::ReplaceItem`, `GetFitSlot`, the PDA, shoulder and
  > blood-bible slots, the Ousters stones, `InitSkillList`,
  > `AddSkillStep`, `GetExpInfo`.
  - Owner (all of 4.x): `gamemodel`'s membership file, the M0–M2 include
    rules, R4 shrinking; `test_item_table.cpp`, `test_item_core.cpp`,
    `test_player_gear.cpp`, `test_skill_core.cpp`.

---

## Phase 5 — Long tail

- [x] **5.1 Split the debug facilities** so `DebugInfo.h`/`MinTr.h` stop
  gating `packetwire` membership. The task named `SocketAPI.cpp`,
  `DatagramSocket.cpp` and `NPCInfo.cpp`; the real list was
  `tests/arch/packetwire_holdouts.txt`, and the task became "take the
  holdouts in".
  > **Status:** done (2026-09-09, fifth slice; PR #144). **Every `.cpp`
  > under `Client/Packet` is a `packetwire` member**; the holdouts file
  > lists nothing and stays only because W0 reads it and the next
  > holdout needs somewhere to be written down. The last one,
  > `RequestClientPlayerManager.cpp`, went behind nine `WireHost`
  > entries and was deleted four hours later: the review
  > of the move found that the whisper mode it served was compiled out
  > upstream (`0 &&` around every writer of the queue and every
  > whisper-mode `Connect()` - see *A reference on a live line can
  > still be dead*), and reading on from there showed the whole
  > outbound peer side - this client dialling other clients for a
  > profile or a whisper - was dead with it: `GCRequestedIPHandler`
  > began `if (bKorean == false || 1) return;`, so no peer address ever
  > arrived from the server. 5.2's seventh and eighth slices deleted the
  > lot (same PR); what the request-service family keeps in the library
  > is the **inbound** side - `RequestServerPlayer` and its manager,
  > peers dialling this client - and `WireHost` is down to **11
  > entries**, the fourth slice's clock and three file-sender calls
  > among them. The move itself is in the PR's first commit for anyone
  > who needs the seam again. **Kept from the work, both test-first:**
  > `CRWhisper::isSlayer()` compared against `RACE_VAMPIRE`;
  > `CRWhisper::write` checked every length and not the race and now
  > refuses one outside the three before writing a byte (the packet is
  > still received; `test_crwhisper.cpp`). The host installers in
  > `GameInit.cpp` and the tests are **designated initialisers**
  > (C++20); the "checked by reading" caveat the fourth slice recorded
  > is retired.
  > Done before that (PRs #63, #64, #74, #75): the logging facility
  > (`DebugLog.{h,cpp}`) lives in `basic/`, so every library may log and
  > the one object is no longer compiled into two libraries; `DebugInfo.h`
  > is one `#include "MinTr.h"` above two no-op macros and nothing in the
  > wire layer includes it. **`Client/Packet/WireHost.h`** declares what
  > the wire layer asks of the program around it — the three
  > `ClientConfig` tuning values, the millisecond clock, the encrypt-seed
  > inputs (zone id, server number, region flags), and three calls on the
  > peer file-transfer manager (six, and an in-game test, until the
  > eighth slice), which stays executable-side because it writes the
  > profile directory and reads the UI — and the
  > executable fills it in beside the other two hosts in `GameInit`; every
  > tuning accessor answers without a host with the value `ClientConfig`'s own
  > constructor sets, and `WIRE_DEFAULT_*` keeps the executable's
  > fallbacks from drifting. `SendBugReport` is `WireHost.cpp`'s second
  > half. `setEncryptCode()` **is live code** (`Encrypter.h` defines
  > `__USE_ENCRYPTER__`; `GCUpdateInfoHandler` calls it on every login),
  > and `WireEncryptSeed`'s four region branches collapse to two distinct
  > expressions, which a test asserts across every server number.
  > Fixed test-first: `Player`'s socket constructor never set
  > `pHashTable`, which `delKey()` (two live callers, both on every
  > reconnect) `delete[]`s — both constructors set it, the destructor
  > frees it, `setKey` frees the previous table; `Wire::ReceiveMyRequest`
  > and `SendOtherRequest` no longer declare `throw ()` over a manager
  > that throws by design. Seven `OUTPUT_DEBUG` blocks and `ProcessMode`'s
  > commented-out body were deleted rather than moved, because R4 counts
  > a `g_p*` name on a dead line as readily as on a live one.
  > **Was untested by construction** until the fifth slice's review: the
  > host installation in `GameInit.cpp` was positional, so transposing
  > two same-signature entries in `s_WireHost` compiled and passed the
  > whole suite; it is designated now, and the forwarder test proves
  > that each `WireHost.cpp` forwarder calls its matching member. **Known, not fixed:** `SocketImpl`'s default
  > constructor is the only one of its four that leaves `m_key` unset;
  > `Player::processCommand`, `processInput`, `processOutput`,
  > `sendPacket`, `disconnect` and `toString` dereference the socket or a
  > stream the default constructor leaves NULL.
  - Owner: W0 over the (empty) holdouts file, so a new `Client/Packet`
    source is a library member unless a line there says why not;
    `test_wire_host.cpp`, `test_player_base.cpp` and their
    address-taking link proofs (non-virtual members — see *What the
    review rounds settled*).

- [x] **5.2 Dead/duplicate source removal** (code-health priority 3).
  > **Status:** done for what the task named (PRs #49, #50, #52, #62).
  > Deleted: the tracked `_bak` item table and the two other `InitItem2`
  > twins (one of which the Windows `VS_UI` target was still compiling);
  > the server's 23,000-line `__INIT_ITEM__` item data and 8,500-line
  > `MitemTableInit.cpp`; the 2,300-line `#ifndef __GAME_CLIENT__` skill
  > data in `MSkillInfoTable.cpp`; and the exclusion graveyard whole
  > (`GlobalVariables`, `MissingGlobals`, `GameHelpers`, `GameFunctions`,
  > `GamePacketFunctions`, `ActionFunctions`, the pre-port `GCNotifyWin`
  > trio) after a scan of all compiled translation units showed none of
  > its 171 function definitions was the only one. The client has always
  > loaded `Item.inf` and `SkillInfo.inf`; git history keeps the data.
  > Closes the review's Medium dead-code finding.
  > **Sixth slice (2026-09-09):** deleted `VS_UI/WinMain.cpp` (3,799
  > lines; a second `WinMain` that drove the widget system without ever
  > calling `InitGame()`, excluded from every configuration by two
  > `list(FILTER)` rules that went with it, and named by the ratchet
  > script's R4 member list, which no longer needs to) and the four
  > files of `Client/OtherClass/` (two request-side packet factory
  > managers, 1,700 lines, in no glob and referenced by nothing; the
  > R12 conformance slice had cleaned 32 exception specifications in
  > files that were never compiled). R1 and R4 unchanged, because
  > neither was ever built - the build is the owner, as before. The two
  > `^Client/`-anchored filters the earlier list named were already
  > gone.
  > **Candidates for a next slice:** the ~186 `__GAME_SERVER__`/
  > `__GAME_CLIENT__` conditionals in the packet sources (2.4), which
  > the include checker evaluates as dead; `#ifndef __GAME_CLIENT__`
  > residue outside `Client/Packet` — `RankBonusTable.cpp` (a save
  > path), `MSectorInfo.h` (portal fields), `Updater/Update.cpp`;
  > `MSkillDomain::SaveToFile`/`LoadFromFile`, which have no caller but
  > are reached through `CTypeTable`'s file I/O template, so deleting
  > them is a template question first. (The peer-to-peer whisper path
  > this list once named is the seventh slice below.)
  > **Seventh slice (2026-09-09, on PR #144, at the user's request):**
  > the peer-to-peer whisper path, which upstream compiled out (`0 &&`
  > around every writer of `WhisperManager`'s queue and every
  > whisper-mode `Connect()`) and 5.1's fifth slice found. Deleted:
  > `WhisperManager.{h,cpp}` whole (its one live member sent a
  > `CGWhisper`, which `UIMessageManager` now sends itself), its
  > construction, deletion and `Update` calls, the whisper `case` in
  > `ProcessMode` and in the connection thread's failure path, the
  > whisper branches of `GCRequestedIPHandler` and
  > `GCRequestFailedHandler`, and seven `WireHost` entries (the
  > character's world and race, the four queue calls, the request-user
  > notification) with their forwarders, installers and tests. R1
  > 488 → 487. `REQUEST_CLIENT_MODE_WHISPER` stays in its enum (the
  > receiving side still names the mode; the values are shared
  > vocabulary - `REQUESTING_FOR_WHISPER` went with its enum in the
  > eighth slice), as do
  > `CRWhisper` and its handler (a Korean-build peer can still whisper
  > *to* this client, and `tests/wire-layout.txt` pins the packet).
  > **Found on the way:** `GCRequestedIPHandler::execute` begins
  > `if (bKorean == false || 1) return;`, so the reply to a
  > `CGRequestIP` is never acted on and a peer's address is learned
  > only from a `CRWhisper` it sends us - the profile fetch, the one
  > peer path left, can dial only a peer that has already whispered.
  > Whether that is worth keeping, or the whole outbound peer side
  > should follow the whisper path, was put to the user, who said
  > remove it.
  > **Eighth slice (2026-09-09, on PR #144):** the rest of the
  > outbound peer side. Deleted: `RequestClientPlayer.{h,cpp}` and
  > `RequestClientPlayerManager.{h,cpp}` from `packetwire` (526 → 524
  > members); the three handlers that took that player
  > (`RCConnectVerify`, `RCRequestVerify`, `RCRequestedFile`) and their
  > registry lines (R1 487 → 484; their handler classes stay declared
  > in the packet headers like every other handler-less packet's); the
  > receive half of `RequestFileManager` (`ReceiveFileInfo`,
  > `RequestReceiveInfo`, the "my request" map and its four calls, the
  > empty `Update`); the dialling half of `ProfileManager`
  > (`RequestProfile`, the require map, `Update`, `CGRequestIP`) and its
  > three callers in `MPlayer`, `GCPartyJoinedHandler` and the
  > `/profile` chat command - the map now holds what `InitProfiles`
  > finds in the profile directory, which is what the three UI readers
  > and the inbound file sender read; the requesting half of
  > `RequestUserManager` (the second map, `REQUESTING_FOR`,
  > `RemoveRequestUser`, `RemoveRequestUserLater`, `Update`) and the
  > `Status`/`TCPPort` fields nothing read - what stays is the address
  > book `CRWhisperHandler` and the three UDP party handlers write and
  > the party datagrams read for their port, inert in a fleet built from
  > this source (nothing here creates an entry); the "no profile"
  > marker (`AddProfileNULL`/`HasProfileNULL`), whose only writer was a
  > deleted handler, and the `VS_UI_GameCommon.cpp` branch that cached
  > it; `RequestConnect`; six `WireHost` entries (`InGameMode`, the
  > three "my request" calls, and the fifth slice's two) with
  > forwarders, installers and tests; the per-frame `Update` calls of
  > all four managers in `GameMain` (`RequestFileManager`'s was empty).
  > **The commit's first message said nothing this deletes could run.
  > That was wrong**, and the review said so: `ProfileManager::Update`
  > ran every ~330 ms and, for a requested name with no known address,
  > sent `CGRequestIP` to the game server on every turn - `bKorean` is
  > true by default - and against a server answering `GCRequestFailed`
  > that was a 3 Hz re-request loop per name, since only a profile
  > arriving cleared the request; and a peer that had whispered to this
  > client first (an original Korean client) could be dialled for its
  > profile. What changes on the wire: **this client no longer sends
  > `CGRequestIP`**, and no longer dials anyone.
  > `GCRequestedIPHandler` and `GCRequestFailedHandler` are explicit
  > no-ops rather than deregistered, because an unregistered id throws
  > `InvalidProtocolException` on the *game server* connection.
  > `RequestDisconnect` stays (a peer's `CRDisconnect` reaches it) and
  > its log line printed the name with `%d`. Kept on purpose: the enum
  > values, `CRWhisper`/`CRConnect`/`CRRequest` and the `RC*` packet
  > classes (wire layout pinned), the four `RC*` handlers the UDP party
  > channel receives (`RCPositionInfo`, `RCCharacterInfo`, `RCStatusHP`,
  > `RCSay`), and the inbound side whole. Owner: the build, W0 over the
  > membership file, and the profile UI in `VS_UI_GameCommon.cpp`
  > reading only what `InitProfiles` provides - which a live server
  > shows as one's own profile image still displaying, and a packet
  > capture as no `CGRequestIP` leaving the client.
  - Owner: the build (nothing deleted was compiled, so R1 held at each
    step); the wrong-file-edited trap is closed for the files named.

- [x] **5.3 TextSystem stub retirement.** Split `TextService.cpp`'s pure
  text utilities from its `g_pLast` drawing entry point.
  > **Status:** done (2026-09-03, PR #65). `TextService::RenderText` — a
  > compatibility shim for `SDL_RenderText` that draws through `g_pLast`
  > — is defined in `Client/TextServiceScreen.cpp`, which the executable
  > compiles; `TextService` has no virtuals, so the split costs no
  > vtable. A split rather than a host, because the thing behind this
  > seam is the drawing surface itself: a host would move the reach, not
  > remove it. **`tests/stubs/` is gone**; `tests/CMakeLists.txt` says
  > why in its place: a library that needs something from the program
  > around it asks through a host struct a test can install, never
  > through a symbol a test binary has to invent. `unit_tests` links
  > `TextSystem` with no stub translation unit — that link is the proof.
  > Whether the client still draws its FPS counter and debug overlays is
  > what running it shows.
  - Owner: the `unit_tests` link line.

- [x] **5.4 Format-string audit** (code-health C19/C20/C22: `sprintf`
  sites whose format is a `Data/Info/String.inf` entry).
  > **Status:** done (2026-09-03/04, PRs #68 through #73); **finding C19
  > is closed**, and its entry in the code-health review lists the five
  > measurements it rests on (R7, R8, the arity audit, and two hand
  > sweeps) rather than a ratchet reading zero. **321 call sites** are
  > converted across `Client`, `VS_UI` and `Client/PacketHandler`.
  > **`basic/SafeFormat.{h,cpp}`** is the checked formatter: a conversion
  > consumes the next argument only if that argument's type can satisfy
  > it; a conversion with no argument left, an argument of the wrong
  > type, a `%n`, or a width taken from an argument is copied out as
  > text instead of performed, so `"[System]%s%s%s%s%n"` against one
  > argument prints `[System]hello%s%s%s%n` rather than reading three
  > stack words. The destination is bounded by its own size, and the
  > array overload takes that size from the declaration. It is in
  > `basic` because the call sites are in the executable, `VS_UI` and
  > the packet handlers, and `basic` is the one library all three link.
  > Front ends: `SafeFormat::Format`, `CMessageArray::AddSafeFormat` (a
  > new entry point, because `AddFormat`'s other 18 callers take literal
  > formats; the two share `StoreRow`), `MString::FormatChecked` (library
  > code with tests — `MString::Format` is a varargs printf reached as a
  > method, which no sweep over printf names could see), and
  > `AllocAskMessage` in `VS_UI_ExtraDialog.cpp`, which allocates and
  > formats in one place so the bound cannot drift from the destination.
  > `GetGameString()` answers `""` for an out-of-range lookup rather than
  > the NULL a default-constructed `MString` gives.
  > **The load-time half is separate:** `SanitizeGameStringTable` scrubs
  > `String.inf` as it is read, cannot check arity, and does not run in
  > the default English build. No `String.inf` ships in this repository,
  > so for `LANGUAGE != 3` none of these entries can be checked here; the
  > formatter's run-time refusal is what protects that build.
  > **Defects fixed on the way:** `GCBloodBibleListHandler` formatted
  > `"%3d %s"` into a buffer four bytes too small; `VS_UI_GameCommon.cpp`
  > formatted two `BYTE` coordinates into a `static char[10]` (needs 12);
  > `VS_UI_ExtraDialog.cpp` appended slayer requirement lines with an
  > unbounded `wsprintf(sz_temp + strlen(sz_temp), …)` into a
  > `char[200]`, and its ask dialogs `sprintf`'d table entries carrying
  > `%s` with **no varargs at all** into `new char[strlen(format)+1]`;
  > `C_VS_UI_ASK_DIALOG` never assigned six `ASK_FRIEND_*` rows of
  > `m_sz_question_msg` (upstream commented the assignments out and left
  > the readers live), so opening any friend dialog handed an
  > indeterminate pointer to `sprintf` as a format — server-triggered
  > through `GCFriendChatting`; `MString::operator=` keeps no allocation
  > for an empty string, so `GetString()` came back NULL into
  > `g_pSystemMessage->Add`; `ModifyStatusManager.cpp` built an inner
  > `sprintf` from a table entry and handed the result to a converted
  > outer call (the two-stage shape of C22 — a sweep for it now returns
  > nothing).
  > **Recorded, not fixed:** `C_VS_UI_INFO::GetChinhoLevel` caches its
  > eleven table entries in a function-local `const static char*[11]`
  > while `InitGameStringTable()` runs at least twice per session and
  > deletes every `MString` — stale by construction if populated before
  > the last init; today both inits finish before any in-game UI runs. A
  > NULL entry there now yields `""` and the write is bounded, but a
  > dangling non-NULL pointer is indistinguishable from a live one.
  > **Thirty-nine** table lookups with a computed subscript remain (15
  > in `Client`, 24 in `VS_UI`, counting every subscript that is not a
  > bare identifier). Latent in the instruments: `SafeFormat::FormatV`
  > used directly at a call site is invisible to the audit and R7 (its
  > only use is inside `CMessageArray`); R7's comment stripping is not
  > string-literal aware where the audit's is; the audit's parenthesis
  > matcher is not literal-aware where its argument splitter is.
  - Owner: R7 and R8 (the pair); `check_format_arity.pl` with both its
    floors; `test_safe_format.cpp`, the `FormatChecked` tests.

---

## First candidate (decided 2026-09-01)

**Task 1.1: the `packetwire` library.** Chosen over the alternatives
because:

1. **Zero-edit move.** Every member file's includes were scanned; none
   reached game code, so the move commit was pure CMake — the safest
   possible first step for agents to execute and review.
2. **It sits under the #1 open risk.** The info classes *are* the parsers
   the GC packets delegate server-supplied data to; the streams are where
   every length check ultimately lands. Tests aimed there have the highest
   defect yield per hour — and the first known defect was already queued
   (task 1.3, the `StringStream` float/double stack overflow, already
   proven real on the server's identical copy).
3. **It unblocks Phase 2.** The dispatcher, and eventually all 700+ packet
   classes, land in this target; creating it first meant every later phase
   was "grow the membership list", not "invent a target".

Runner-up considered and deferred: extracting `ExperienceTable`-class pure
tables (4.1) — testable, but it neither touched the top risk nor unblocked
anything else.
