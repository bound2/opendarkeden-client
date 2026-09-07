//----------------------------------------------------------------------
// test_packet_id_set.cpp
//----------------------------------------------------------------------
//
// PacketIDSet::deletePacketID - what the set holds afterwards, read back
// through hasPacketID and toString. The read half and addPacketID's
// duplicate refusal are in test_cpp20_container_helpers.cpp.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "PacketIDSet.h"
#include "PlayerStatus.h"
#include "Packet.h"
#include "Exception.h"

#include <string>

namespace {

// The set exposes no size, so the count comes out of toString(): every
// id, space separated, after "PacketID:" and before the ')'.
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

// Whether deleting an id the set really holds is accepted.
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
	// Guard: without a working delete of a present id, the call below is
	// an undefined erase(end()) that would abort the whole suite.
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
