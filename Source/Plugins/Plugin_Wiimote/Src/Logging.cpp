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



//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// -------------
#include <string>
#include <stdio.h>
#ifdef _WIN32
	#include <windows.h>
#endif

#include "StringUtil.h"

#define HAVE_WX 1
#if defined(HAVE_WX) && HAVE_WX // wxWidgets
	#include <wx/datetime.h> // for the timestamps
#endif
///////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Settings
// -------------

// On and off
bool g_consoleEnable = true;
bool gSaveFile = true;
#define DEBUG_WIIMOTE // On or off
const int nFiles = 1;

// Create handles
#ifdef DEBUG_WIIMOTE
	FILE* __fStdOut[nFiles];
#endif
#ifdef _WIN32
	HANDLE __hStdOut = NULL;
#endif

//////////////////////////////


// =======================================================================================
/* Get Timestamp */
// -------------
std::string Tm(bool Ms)
{
	#if defined(HAVE_WX) && HAVE_WX
		std::string Tmp;
		if(Ms)
		{
			wxDateTime datetime = wxDateTime::UNow(); // Get timestamp
			Tmp = StringFromFormat("%02i:%02i:%03i",
			datetime.GetMinute(), datetime.GetSecond(), datetime.GetMillisecond());
		}
		else
		{
			wxDateTime datetime = wxDateTime::Now(); // Get timestamp
			Tmp = StringFromFormat("%02i:%02i",
			datetime.GetMinute(), datetime.GetSecond());
		}
		return Tmp;
	#else
		std::string Tmp = "";
		return Tmp;
	#endif
}
// ===========================


// ---------------------------------------------------------------------------------------
// File printf function
// ---------------
int PrintFile(int a, char *fmt, ...)
{
#if defined(DEBUG_WIIMOTE) && defined(_WIN32)
	if(gSaveFile)
	{
		char s[500]; // WARNING: mind this value
		va_list argptr;
		int cnt;

		va_start(argptr, fmt);
		cnt = vsnprintf(s, 500, fmt, argptr); // remember to update this value to
		va_end(argptr);

		// ---------------------------------------------------------------------------------------
		if(__fStdOut[a]) // TODO: make this work, we have to set all default values to NULL
			//to make it work
			fprintf(__fStdOut[a], s);
			fflush(__fStdOut[0]); // Write file now, don't wait
		// -------------

		return(cnt);
	}
	else
	{
		return 0;
	}
#else
	return 0;
#endif
}
