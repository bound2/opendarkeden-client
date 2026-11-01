---
type: Rule
title: "Server link"
description: "The game connection admits only the packets a game server sends: in CPS_NORMAL the validator's set is folded from the factory table, every registered GC packet and nothing else, and the game fuzz targets read that set unless DE_FUZZ_ANY_ID widens it to every registered id."
tags: [protocol, security]
enforced_by: [unit_tests, fuzz_replay_client_stream_any_id]
---

# Server link

| Rule | Owner | What fails when it is broken |
|------|-------|------------------------------|
| The game connection admits only what a game server sends: `PacketValidator::init` folds `GameServerLink::SentByAGameServer` (the packet name's link prefix, `GC`) over the factory table into the `CPS_NORMAL` set, so every registered GC packet is admitted there and no other registered id and no unregistered one is | `GameServerLink` and `ClientPlayerReceiveLoop` in `unit_tests` (`tests/unit/test_game_server_link.cpp`) | the test prints each id and name admitted or refused against the rule; a golden or a handler seed the game targets replay that the gate refuses; a client-to-server frame the production loop reads instead of refusing |
| A registered factory carries its packet's name in every build | `DE_PACKET_FACTORY` in `PacketFactoryManager::init` (`Client/Packet/PacketFactoryManager.cpp`), read back by `getPacketName` | a factory added without its name is nameless, and the link refuses it in game |

**The game connection admits only the packets a game server sends.** `CPS_NORMAL`, the status
of a session in the game, used to hold a `PIST_ANY` set: every one of the 287 factories
`PacketFactoryManager::init` registers (2026-11-01: GC 246, LC 17, CR 4, RC 7, CG 11, UC 1,
CU 1) was read there, and so was every unregistered id, as far as `createPacket`, which refused
it. Since task 17 of the [next work plan](../reviews/NEXT.md) the set is folded from the factory
table by `GameServerLink::SentByAGameServer` (`Client/Packet/GameServerLink.h`): a packet whose
name's link prefix is `GC`, game server to client, is admitted, and nothing else is. The 11 CG
packets the client only writes, the login server's 17 LC replies, a peer's CR and RC packets and
the updater's UC and CU (read by handlers that do nothing, sent by no server) are refused before
their body is read, on the path a refused id already took in the login statuses:
`ClientPlayer::processCommand` throws `InvalidProtocolException("invalid packet ORDER")`, which
`UpdateSocketInput` (`Client/GameMain.cpp`) reports to the server as a bug report and ends the
session on. A new GC packet is admitted once it is registered; a packet registered with another
prefix is not, whatever handler it has. `GCExchangeList` is registered with no handler, so it is
admitted here and refused by name at the dispatch, as before. The server narrows its side of
the same link the same way: its `src/Core/GameClientLink.h` and its `docs/protocol/client-link.md`.

**A registered factory carries its packet's name.** A factory states `getPacketName` only
under `__DEBUG_OUTPUT__`, and so does a packet, so no name reaches the validator through either
base class in a normal build. `init` therefore registers each factory with
`DE_PACKET_FACTORY(Name)`, which gives `new NameFactory()` and `"Name"` from one token, and
`PacketFactoryManager::getPacketName` answers that name in every build; `addFactory` with a
factory alone (the fakes some tests add) records the factory's own name under `__DEBUG_OUTPUT__`
and none otherwise, which the link refuses.

**The fuzz targets.** The game stream target and the game handler target read in `CPS_NORMAL`
through the validator, so they read the GC packets the link admits; with `DE_FUZZ_ANY_ID` in the
environment `tests/fuzz/client_stream_reader.h` puts a `PIST_ANY` set back, so the readers of the
refused ids stay fuzzed, and the ctest `fuzz_replay_client_stream_any_id` replays every golden
that way ([fuzzing](fuzzing.md)). The seed corpora were never wider than the admitted set: the
game targets' corpora are built from the GC goldens, and the handler seeds carry GC frames,
which `tests/unit/test_game_server_link.cpp` checks.
