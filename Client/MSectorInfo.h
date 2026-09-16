//----------------------------------------------------------------------
// MSectorInfo.h
//----------------------------------------------------------------------
// 안전지대 표시.. 등등..
// 자주 바뀔거 같아서.. map화일과 분리한다.
//----------------------------------------------------------------------


#ifndef	__MSECTORINFO_H__
#define	__MSECTORINFO_H__

#pragma warning(disable:4786)


//----------------------------------------------------------------------
// Flag
//----------------------------------------------------------------------
#define FLAG_SECTOR_SAFETY					0x01	// safety zone

class MSectorInfo {
	public :
		MSectorInfo();
		~MSectorInfo();

		void operator = (const MSectorInfo& s)
		{
			m_fProperty = s.m_fProperty;
		}


		//------------------------------------------------
		//
		//                  File I/O
		//
		//------------------------------------------------
		void	SaveToFile(std::ofstream& file);
		void	LoadFromFile(std::ifstream& file);

		//------------------------------------------------
		// Safety
		//------------------------------------------------
		BYTE	IsSafety() const		{ return m_fProperty & FLAG_SECTOR_SAFETY; }
		void	SetSafety()				{ m_fProperty |= FLAG_SECTOR_SAFETY; }
		void	UnSetSafety()			{ m_fProperty &= ~FLAG_SECTOR_SAFETY; }

	protected :
		// 정보 Flag
		BYTE					m_fProperty;

};

#endif


