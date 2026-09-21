#pragma once

#include "MString.h"
#include "RaceType.h"
#include <fstream>
#include <string_view>
#include <vector>

class MHelpMessage {
public:
	enum MESSAGE_TYPE { MESSAGETYPE_NORMAL, MESSAGETYPE_OPEN, MESSAGETYPE_SAFESECTOR_OPEN };
	int m_messageType;
	MString m_strKeyword;
	MString m_strEvent;
	MString m_strTitle[RACE_MAX];
	int m_iLevelMax[RACE_MAX], m_iLevelLow[RACE_MAX];
	int m_iDomainMax[RACE_MAX], m_iDomainLow[RACE_MAX];
	int m_iAttrMax[RACE_MAX], m_iAttrLow[RACE_MAX];
	int m_iSender[RACE_MAX];
	MString m_strDetail[RACE_MAX];
	MHelpMessage();
	virtual ~MHelpMessage();
	// Slayers use total attributes; other races use level. A lower bound of -1
	// disables its whole interval. Invalid races are rejected before indexing.
	bool IsEligible(int race, int level, long long attributes) const;
};

class MHelpMessageManager {
public:
	// Independent managers start empty; the singleton loads the default resource.
	MHelpMessageManager() = default;
	static MHelpMessageManager& Instance();
	virtual ~MHelpMessageManager() = default;
	const MString& getSender(int index) const { return m_SenderVector.at(index); }
	size_t getSenderSize() const { return m_SenderVector.size(); }
	const MHelpMessage& getMessage(int index) const { return m_MessageVector.at(index); }
	size_t getMessageSize() const { return m_MessageVector.size(); }

	// Complete resource records replace the current collection; failures preserve it.
	bool LoadFromText(std::string_view encoded);
	bool LoadHelpMessageRpk(const char* filename);
	void LoadFromFile(std::ifstream& file);
	void LoadFromFile(const char* filename);
	void SaveToFile(std::ofstream& file);
	void SaveToFile(const char* filename);

private:
	std::vector<MHelpMessage> m_MessageVector;
	std::vector<MString> m_SenderVector;
	bool LoadUtf8(std::string_view text);
	bool Serialize(std::string& output) const;
};
