//---------------------------------------------------------------------------
// MHelpStringTable.cpp
//---------------------------------------------------------------------------
#include "Client_PCH.h"
#include "MHelpStringTable.h"

//---------------------------------------------------------------------------
// Global
//---------------------------------------------------------------------------
MHelpStringTable*	g_pHelpStringTable = NULL;

//---------------------------------------------------------------------------
//
// constructor / destructor
//
//---------------------------------------------------------------------------
MHelpStringTable::MHelpStringTable()
{
}

MHelpStringTable::~MHelpStringTable()
{
}

//---------------------------------------------------------------------------
//
// member functions
//
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Init
//---------------------------------------------------------------------------
void		
MHelpStringTable::Init( int size )
{
	MStringArray::Init( size );

	m_Displayed.Init( size );

	ClearDisplayed();
}

//---------------------------------------------------------------------------
// Clear Displayed
//---------------------------------------------------------------------------
void		
MHelpStringTable::ClearDisplayed()
{
	for (int i=0; i<m_Displayed.GetSize(); i++)
	{
		m_Displayed.Set(i, false);
	}
}

//---------------------------------------------------------------------------
// Load From File
//---------------------------------------------------------------------------
void			
MHelpStringTable::LoadFromFile(std::ifstream& file)
{
	MStringArray::LoadFromFile( file );

	m_Displayed.Init( m_Size );

	ClearDisplayed();
}

//---------------------------------------------------------------------------
// operator []
//---------------------------------------------------------------------------
const MString&
MHelpStringTable::operator [] (int type)		
{
	m_Displayed.Set(type, true);
	return MStringArray::Get(type);
}

//---------------------------------------------------------------------------
// get
//---------------------------------------------------------------------------
const MString&
MHelpStringTable::Get(int type)				
{ 
	m_Displayed.Set(type, true);
	return MStringArray::Get(type);
}
