//----------------------------------------------------------------------
// MNPCTableEnglish.h
//----------------------------------------------------------------------
// The English names and role descriptions for Data/Info/NPC.inf, which
// ships in Korean. The table itself is generated - see MNPCTableEnglish.cpp.
//----------------------------------------------------------------------

#ifndef __MNPCTABLEENGLISH_H__
#define	__MNPCTABLEENGLISH_H__

//----------------------------------------------------------------------
// Replace the loaded NPC names and descriptions with the English text.
// Call after MNPCTable::LoadFromFile(), and only when the client is
// running in English - see UseEnglishText().
//----------------------------------------------------------------------
extern void				ApplyEnglishNPCTable();

#endif
