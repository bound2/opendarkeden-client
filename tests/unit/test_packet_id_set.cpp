//----------------------------------------------------------------------
// test_packet_id_set.cpp
//----------------------------------------------------------------------
//
// PacketIDSet - the per-status set of packet ids the packet validator
// keeps, one instance per PlayerStatus.
//
// tests/unit/test_cpp20_container_helpers.cpp already covers the read
// half (hasPacketID under each of the four set types) and the duplicate
// refusal in addPacketID. This file covers deletePacketID, which had
// nothing on it at all: its membership test was inverted, so it threw
// NoSuchElementException for an id the set really held and fell through
// to std::set::erase(end()) for one it did not.
//
// The contract asserted here is the observable one - what the set holds
// afterwards, read back through hasPacketID and toString - rather than
// the spelling of the test inside the function.
//
// Note on the guard in DeleteOfAnAbsentIDThrowsAndLeavesTheSetAlone:
// the absent-id path was undefined behaviour before the fix, and MSVC's
// debug iterators abort the process on erase(end()), which would take
// the whole suite down instead of reporting one failed test. That test
// therefore establishes that a delete of a present id works before it
// enters the path, and reports rather than crashes when it does not.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "PacketIDSet.h"
#include "PlayerStatus.h"
#include "Packet.h"
#include "Exception.h"

#include <string>

namespace {

//----------------------------------------------------------------------
// The set exposes no size of its own, so the count comes out of the one
// public view of its contents: toString() renders every id it holds,
// space separated, after the "PacketID:" label and before the closing
// parenthesis.
//----------------------------------------------------------------------
int		IDCount(const PacketIDSet& idSet)
{
	const std::string				rendered	= idSet.toString();
	const std::string::size_type	label		= rendered.find("PacketID:");

	if (label == std::string::npos)
		return -1;

	int		count		= 0;
	bool	bInToken	= false;

	for (std::string::size_type i = label + 9; i < rendered.size(); i++)
	{
		const char	c = rendered[i];

		if (c == ')')
			break;

		if (c == ' ')
			bInToken = false;
		else if (!bInToken)
		{
			bInToken = true;
			count++;
		}
	}

	return count;
}

//----------------------------------------------------------------------
// Whether deleting an id the set really holds is accepted. Used as a
// guard by the absent-id test, on its own throwaway set.
//----------------------------------------------------------------------
bool	DeleteOfAPresentIDIsAccepted()
{
	PacketIDSet	probe(CPS_NONE, PacketIDSet::PIST_NORMAL);

	probe.addPacketID((PacketID_t)1);

	try {
		probe.deletePacketID((PacketID_t)1);
	} catch (Throwable&) {
		return false;
	}

	return true;
}

} // namespace

//----------------------------------------------------------------------
// deletePacketID - the id it was given goes, and nothing else does
//----------------------------------------------------------------------
TEST(PacketIDSet, DeleteRemovesThePresentID)
{
	PacketIDSet	idSet(CPS_NONE, PacketIDSet::PIST_NORMAL);

	idSet.addPacketID((PacketID_t)11);
	idSet.addPacketID((PacketID_t)22);
	CHECK_EQ(2, IDCount(idSet));

	// An id the set holds is deleted, not refused.
	bool	bThrew = false;
	try {
		idSet.deletePacketID((PacketID_t)11);
	} catch (Throwable&) {
		bThrew = true;
	}
	CHECK_EQ(false, bThrew);

	CHECK_EQ(false, idSet.hasPacketID((PacketID_t)11));
	CHECK(idSet.hasPacketID((PacketID_t)22));
	CHECK_EQ(1, IDCount(idSet));

	// The id is genuinely gone: adding it again is not a duplicate,
	// while adding the survivor still is.
	bThrew = false;
	try {
		idSet.addPacketID((PacketID_t)11);
	} catch (DuplicatedException&) {
		bThrew = true;
	}
	CHECK_EQ(false, bThrew);
	CHECK_EQ(2, IDCount(idSet));

	bThrew = false;
	try {
		idSet.addPacketID((PacketID_t)22);
	} catch (DuplicatedException&) {
		bThrew = true;
	}
	CHECK(bThrew);
}

//----------------------------------------------------------------------
// deletePacketID - an id the set never held is refused, and refusing it
// changes nothing
//----------------------------------------------------------------------
TEST(PacketIDSet, DeleteOfAnAbsentIDThrowsAndLeavesTheSetAlone)
{
	// Guard, not the subject of this test: see the file header. Without
	// a working delete of a present id, the call below is the undefined
	// erase(end()) and would abort the suite rather than fail a test.
	const bool	bDeleteIsAccepted = DeleteOfAPresentIDIsAccepted();
	CHECK(bDeleteIsAccepted);
	if (!bDeleteIsAccepted)
		return;

	PacketIDSet	idSet(CPS_NONE, PacketIDSet::PIST_NORMAL);

	idSet.addPacketID((PacketID_t)11);
	idSet.addPacketID((PacketID_t)22);

	bool	bThrew = false;
	try {
		idSet.deletePacketID((PacketID_t)33);
	} catch (NoSuchElementException&) {
		bThrew = true;
	}
	CHECK(bThrew);

	CHECK(idSet.hasPacketID((PacketID_t)11));
	CHECK(idSet.hasPacketID((PacketID_t)22));
	CHECK_EQ(false, idSet.hasPacketID((PacketID_t)33));
	CHECK_EQ(2, IDCount(idSet));

	// The same on an empty set, which has no first element to walk to.
	PacketIDSet	empty(CPS_NONE, PacketIDSet::PIST_NORMAL);

	bThrew = false;
	try {
		empty.deletePacketID((PacketID_t)11);
	} catch (NoSuchElementException&) {
		bThrew = true;
	}
	CHECK(bThrew);
	CHECK_EQ(0, IDCount(empty));
}

//----------------------------------------------------------------------
// deletePacketID - deleting from the middle of the set leaves the ids
// on either side of it
//----------------------------------------------------------------------
TEST(PacketIDSet, DeleteTakesOnlyTheIDItWasGiven)
{
	PacketIDSet	idSet(CPS_NONE, PacketIDSet::PIST_NORMAL);

	idSet.addPacketID((PacketID_t)11);
	idSet.addPacketID((PacketID_t)22);
	idSet.addPacketID((PacketID_t)33);
	CHECK_EQ(3, IDCount(idSet));

	bool	bThrew = false;
	try {
		idSet.deletePacketID((PacketID_t)22);
	} catch (Throwable&) {
		bThrew = true;
	}
	CHECK_EQ(false, bThrew);

	CHECK(idSet.hasPacketID((PacketID_t)11));
	CHECK_EQ(false, idSet.hasPacketID((PacketID_t)22));
	CHECK(idSet.hasPacketID((PacketID_t)33));
	CHECK_EQ(2, IDCount(idSet));

	// Emptying the set one id at a time.
	bThrew = false;
	try {
		idSet.deletePacketID((PacketID_t)11);
		idSet.deletePacketID((PacketID_t)33);
	} catch (Throwable&) {
		bThrew = true;
	}
	CHECK_EQ(false, bThrew);

	CHECK_EQ(false, idSet.hasPacketID((PacketID_t)11));
	CHECK_EQ(false, idSet.hasPacketID((PacketID_t)33));
	CHECK_EQ(0, IDCount(idSet));
}
