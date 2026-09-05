//--------------------------------------------------------------------------------
//
// Filename   : PacketAssert.cpp
//
// Renamed from Assert.cpp - see PacketAssert.h for why.
//
// Written By : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Types.h"
#include "PacketAssert.h"
#include "Exception.h"
#include <time.h>

//--------------------------------------------------------------------------------
//
// __assert__
//
// There is no need to wrap this function in __BEGIN_TRY / __END_CATCH.
//
//--------------------------------------------------------------------------------
void __assert__ ( const char * file , uint line , const char * func , const char * expr )
	throw ( AssertionError )
{
	StringStream msg;
	
	msg << eos
		<< "Assertion Failed : " << file << " : " << line;

	if ( func )
		msg << " : " << func;

	time_t currentTime = time(0);
	
	msg << expr << " at " << ctime(&currentTime);
	
	ofstream ofile("assertion_failed.log",ios::app);
	ofile << msg.toString().c_str() << endl;
	ofile.close();

	throw AssertionError( msg.toString() );
}

//--------------------------------------------------------------------------------
//
// __assert__ - C++20 entry point
//
// What Assert() reaches now. The site was captured at the failing line by
// the defaulted DiagnosticSite rather than forwarded from it, and is handed
// to the function above unchanged, so the logged line and the thrown
// AssertionError's message are byte for byte what they were.
//
// The captured function name is deliberately NOT used: the Windows macro
// passed "" and the others passed __PRETTY_FUNCTION__, and which of the two
// this platform wants is still decided by the macro and passed in func.
//
//--------------------------------------------------------------------------------
void __assert__ ( const char * func , const char * expr , const DiagnosticSite & site )
{
	__assert__( site.file , (uint)site.line , func , expr );
}
