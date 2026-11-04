//----------------------------------------------------------------------
// handler_world_ids.h
//----------------------------------------------------------------------
//
// The ids and cells of what the handler fuzz world makes for the player's
// request (handler_world.h's MakeRequests), shared with the seeds that
// name them (tests/unit/test_fuzz_seeds.cpp), so a seed and the world
// cannot drift apart on an id. Plain integers: the seed test has the
// packet classes, not the model.
//
//----------------------------------------------------------------------

#ifndef __HANDLER_WORLD_IDS_H__
#define __HANDLER_WORLD_IDS_H__

namespace HandlerFuzz {

// The second player beside the player's, which the other-info window
// names (recording_hosts.h's kOtherInfoName).
inline constexpr unsigned	kPeerID = 1001;

// The item the item-check buffer holds in every run: the sword on the
// ground a vampire's player picks up to the mouse, the mixing item a
// slayer's player forges with, the remover on the mouse an ousters' player
// takes an option off with.
inline constexpr unsigned	kBufferedItemID = 2000;

// The two inventory items a slayer's forge names, at kForgeCells, and the
// inventory item an ousters' option removal names, at kInventoryCell,
// where a slayer's mixing item sits too.
inline constexpr unsigned	kForgeItemIDs[2] = { 2001, 2002 };
inline constexpr unsigned	kTargetItemID = 2003;
inline constexpr unsigned	kInventoryCell[2] = { 0, 0 };
inline constexpr unsigned	kForgeCells[2][2] = { { 3, 0 }, { 6, 0 } };

} // namespace HandlerFuzz

#endif
