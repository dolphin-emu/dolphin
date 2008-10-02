// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <stdio.h>
#include <stdarg.h>

#include "Globals.h"


// =======================================================================================
// This is to verbose, it has to be turned on manually for now
// --------------
void DebugLog(const char* _fmt, ...)
{
#if defined(_DEBUG) || defined(DEBUGFAST)
/*	char Msg[512];
    va_list ap;

    va_start( ap, _fmt );
    vsprintf( Msg, _fmt, ap );
    va_end( ap );

    OutputDebugString(Msg);

    g_dspInitialize.pLog(Msg);
 */
#endif
}
// =============


void ErrorLog(const char* _fmt, ...)
{
	char Msg[512];
	va_list ap;

	va_start(ap, _fmt);
	vsprintf(Msg, _fmt, ap);
	va_end(ap);

	g_dspInitialize.pLog(Msg);
#ifdef _WIN32
	::MessageBox(NULL, Msg, "Error", MB_OK);
#endif

	DSP_DebugBreak(); // NOTICE: we also break the emulation if this happens
}


