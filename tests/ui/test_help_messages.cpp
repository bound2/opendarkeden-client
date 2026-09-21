#include "test_framework.h"
#include "Platform.h"
#include "MHelpMessageManager.h"

TEST(HelpMessages, ProductionMessageDefaultsLinkFromTheUiLibrary)
{
	MHelpMessage message;
	CHECK_EQ(MHelpMessage::MESSAGETYPE_NORMAL, message.m_messageType);
	CHECK_EQ(0, message.m_strKeyword.GetLength());
	CHECK_EQ(0, message.m_strEvent.GetLength());
	for (int race = 0; race < RACE_MAX; ++race) {
		CHECK_EQ(-1, message.m_iSender[race]);
		CHECK_EQ(-1, message.m_iLevelLow[race]);
		CHECK_EQ(-1, message.m_iLevelMax[race]);
		CHECK_EQ(-1, message.m_iDomainLow[race]);
		CHECK_EQ(-1, message.m_iDomainMax[race]);
		CHECK_EQ(-1, message.m_iAttrLow[race]);
		CHECK_EQ(-1, message.m_iAttrMax[race]);
		CHECK_EQ(0, message.m_strTitle[race].GetLength());
		CHECK_EQ(0, message.m_strDetail[race].GetLength());
	}
}
