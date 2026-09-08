////////////////////////////////////////////////////////////////////////////////
//	created:	2004/12/22
//	file base:	client_pch.h
//
//	VS_UI's entry to the one precompiled header, Client/Client_PCH.h.
//	This file used to be a second, divergent copy of it - without
//	__GAME_CLIENT__, so the VS_UI translation units that include packet
//	classes saw a different Packet vtable from the library defining
//	them - chosen over the other by include-path order. Only VS_UI's
//	warning pragmas stay here; everything else is the shared header.
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef _MSC_VER
	#pragma warning(disable:4290)
	#pragma warning(disable:4018)
	#pragma warning(disable:4244)
	#pragma warning(disable:4786)
#endif

#include "../Client/Client_PCH.h"
