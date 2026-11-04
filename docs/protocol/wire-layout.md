---
type: Rule
title: "Wire layout"
description: "Every factory's id, name and max body size is pinned in tests/wire-layout.txt and every packet's bytes in tests/golden/*.hex; a layout change is a protocol change shipped identically in the server, re-recorded deliberately, never to make a test pass."
tags: [protocol, testing, server]
enforced_by: [wire_inventory_fresh, unit_tests]
generated: { by: claude-code/claude-fable-5-1, at: 2026-10-21T19:53:16Z }
---

# Wire layout

**Wire layout is pinned.** Any change under `Client/Packet` that touches `read()`, `write()` or
`getPacketMaxSize()` must keep `tests/wire-layout.txt` in sync and must be diffed against the
server with `server/tests/tools/wire_inventory_diff.sh`. A layout change is a protocol change and
needs the identical change in the server repo, with the two commits linked.

# The inventory

`tests/wire-layout.txt` lists packet id, name and max body size for every factory under
`Client/Packet`, produced from the real factory objects by `tests/unit/test_wire_layout.cpp`.
`tests/tools/gen_wire_inventory.pl` writes `tests/generated/WireInventory.inc`, one include and
one registration per factory class; the test constructs every factory and its packet (the link
proof for the written CG/CL directions, which no manager ever creates), checks the ids are unique,
and checks that every id `PacketFactoryManager` registers is a listed factory at the factory's
own size. Re-run the generator after adding or changing a factory (the `wire_inventory_fresh`
ctest fails otherwise) and re-record with `UPDATE_GOLDENS=1`. The server commits the same file
from its own packet classes; a diff between the two is a protocol bug in one repo or the other
(the server's `docs/reviews/RESTRUCTURING.md` task 1.4 holds the findings). The Rpackets factories
are constructed and checked but kept out of the rendered file, because the server deleted its
copies.

**The diff ran clean on 2026-11-04** against the server's `master` at `58913ab`: task 23 of the
[next work plan](../reviews/NEXT.md) moved the 41 `GC` budgets the two inventories disagreed on,
each to what the server's `write()` can emit, which the server's `kMaxSize` states exactly for 40
of them (`GCNPCAskDynamic`'s 16,425 is a budget its `write()` does not enforce: the subject and
the answers are unbounded there, a finding for the server, bounded in its PR #401). Thirty-three rose, where this
client's receive gate (`ClientPlayer::processCommand`, "too large packet SIZE") refused a legal
packet: `GCShopBuyFail`, whose 9-byte body never fit its 4-byte budget; a full guild member
list, union offer list, war list or war schedule; a creature with more than 63 effects, since
`EffectInfo::getMaxSize()` returned the count's range, not the bytes; the effected-creature
lists of eleven skill, bomb and mine replies, budgeted one id instead of 255; `GCSkillInfo`,
budgeted one record of eight; `GCAddInjuriousCreature`, whose read also refused a name past 10
characters where the server writes 20. Eight fell to the server's exact maximum: an item's
option list, which the server's writers cap at `MAX_ITEM_OPTION_NUM` (30, now a client constant
too), in `GCCreateItem`, `GCShopBought`, `GCShopBuyOK`, `GCShopList` and `GCStashList`;
`GCNicknameList`'s 500 entries through a byte count; a byte `GCPetStashList` and `GCReconnect`
budgeted and never read. No byte on the wire moved; `tests/wire-layout.txt` was re-recorded.
The review of the server's side found a term both repos had wrong: `GearInfo` budgeted 13
gear slots where a creature wears up to 22 (`WEAR_MAX`) and the server writes one record per
worn item, so a `GCMorph1` for a player wearing more than 13 items (26,209 budgeted, 29,368
possible) was refused at this gate, and `GCUpdateInfo`'s gear term overflowed into the slack its
never-filled motorcycle slot list leaves. The term counts 22 slots in both repos now (the
server's PR #401), so `GCMorph1`'s line is 29,368 and `GCUpdateInfo`'s 62,305 in both
inventories. Kept in view: this client reads several string and count bytes uncapped (a pet's
nickname, a nickname record, a shop sign, a monster's name, a stash item's sub-item count), so
these budgets hold against a conforming server ([packet input safety](packet-input-safety.md)).

# The goldens

Packet tests construct real packets through the real factories and pin their bytes against
`tests/golden/*.hex` (`tests/unit/test_packet_goldens.cpp` has the recipe and the
`UPDATE_GOLDENS=1` re-record rule). **Do not re-record a failing golden merely to make a test
pass**: a byte diff is a protocol change the other repo must ship identically. 144 of the files
share a name with a server golden and 122 are byte-identical copies of it (measured 2026-09-29
against the server's `c1157eb1`, with the two `LCPCList` goldens). The other 22 differ: the server
re-recorded `CGMove` and `GCMoveOK` (every code, and the framed one), `CGSay` and `CGWhisper`
with stronger fixtures on 2026-09-16 (its `93883b4f`), and `CGBloodDrain`, `CGSkillToNamed`,
`CLLogin`, `GCAddItemToItemVerify.threeenchant`, `GCGuildChat`, `GCSay` and `GCSystemMessage`
hold other fixture values in the two repos. Distinguish a fixture difference from a layout change.
A slice that adds a shared golden moves these counts; `diff -r` of the two golden directories is
the cross-repo wire check.
