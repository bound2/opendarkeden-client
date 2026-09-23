//----------------------------------------------------------------------
// CFilterPack.cpp
//----------------------------------------------------------------------

#include "CFilter.h"
#include "CFilterPack.h"
#include "DebugLog.h"
#include <memory>
#include <fstream>

//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------

CFilterPack::CFilterPack()
{
	m_nFilters = 0;
	m_pFilters = NULL;
}

CFilterPack::~CFilterPack()
{
	// array를 메모리에서 제거한다.
	Release();
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Init
//----------------------------------------------------------------------
void
CFilterPack::Init(TYPE_FILTERID count)
{
	std::unique_ptr<CFilter[]> pending(count ? new CFilter[count] : nullptr);
	Release();
	m_pFilters = pending.release();
	m_nFilters = count;
}


//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
CFilterPack::Release()
{
	if (m_pFilters != NULL)
	{
		// 모든 CFilter를 지운다.
		delete [] m_pFilters;
		m_pFilters = NULL;
		
	}
	m_nFilters = 0;
}

//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
void
CFilterPack::SaveToFile(ofstream& file)
{
	// 개수 저장
	file.write((const char*)&m_nFilters, SIZE_FILTERID);

	// 저장된 것이 없으면 return
	if (m_nFilters==0 || m_pFilters==NULL)
		return;

	// 각각의 Filter를 File에 저장한다.
	for (TYPE_FILTERID i=0; i<m_nFilters; i++)
	{
		m_pFilters[i].SaveToFile( file );
	}
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
bool
CFilterPack::LoadFromFile(ifstream& file)
{
	try {
		TYPE_FILTERID count = 0;
		if (!file.read(reinterpret_cast<char*>(&count), SIZE_FILTERID)) {
			LOG_ERROR("Rejected light-filter pack header");
			return false;
		}
		const auto start = file.tellg();
		if (start == std::streampos(-1) || !file.seekg(0, std::ios::end)) return false;
		const auto end = file.tellg();
		if (end < start || !file.seekg(start)) return false;
		// Every nonempty filter needs two dimension words and at least one byte.
		if (static_cast<std::streamoff>(count) > (end - start) / 5) {
			LOG_ERROR("Rejected light-filter pack count: count=%u", unsigned(count));
			return false;
		}
		std::unique_ptr<CFilter[]> pending(count ? new CFilter[count] : nullptr);
		for (unsigned i = 0; i < count; ++i) {
			const auto offset = file.tellg();
			if (!pending[i].LoadFromFile(file) || !file) {
				LOG_ERROR("Rejected light-filter pack entry: id=%u offset=%lld", i,
					static_cast<long long>(static_cast<std::streamoff>(offset)));
				return false;
			}
		}
		Release();
		m_pFilters = pending.release();
		m_nFilters = count;
		return true;
	} catch (...) {
		LOG_ERROR("Cannot read light-filter pack");
		return false;
	}
}
