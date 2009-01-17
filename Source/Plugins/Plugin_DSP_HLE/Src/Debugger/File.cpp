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


// --------------------
// Includes
#include <string>
#include <stdio.h>
#include "Common.h"
#ifdef _WIN32
#include <windows.h>
#endif

#if defined(HAVE_WX) && HAVE_WX
#include "../Debugger/Debugger.h"
#include "../Debugger/File.h"
extern CDebugger* m_frame;
#endif

// --------------------
// On and off
bool g_consoleEnable = true;
//int gSaveFile = 0;
#define DEBUG_HLE

// --------------------
// Create file handles
#ifdef DEBUG_HLE
	FILE* __fStdOut[nFiles];
#endif


// =======================================================================================
/* Open file handles */
// -------------
void StartFile(int width, int height, char* fname)
{
#if defined(DEBUG_HLE) && defined(_WIN32)
	if(fname)
	{
		for(int i = 0; i < nFiles; i++)
		{
			// Edit the log file name
			std::string FileEnding = ".log";
			std::string FileName = fname;
			char buffer[33]; _itoa(i, buffer, 10); // convert number to string
			std::string FullFilename = (FileName + buffer + FileEnding);
			__fStdOut[i] = fopen(FullFilename.c_str(), "w");
		}
	}
#endif
}
// ======================


//////////////////////////////////////////////////////////////////////////////////////////
/* Close the file handles */
// -------------
void CloseFile()
{
	 // Close the file handles
	for(int i = 0; i < nFiles; i++)
	{
		if(__fStdOut[i]) fclose(__fStdOut[i]);
	}
}
//////////////////////////////


// ---------------------------------------------------------------------------------------
// File printf function
// -------------
int PrintFile(int a, char *fmt, ...)
{
#if defined(DEBUG_HLE) && defined(_WIN32)

	if(m_frame->gSaveFile)
	{
		char s[StringSize]; 
		va_list argptr;
		int cnt;

		va_start(argptr, fmt);
		cnt = vsnprintf(s, StringSize, fmt, argptr);
		va_end(argptr);

		// ---------------------------------------------------------------------------------------
		if(__fStdOut[a]) // TODO: make this work, we have to set all default values to NULL
			// to make it work
			fprintf(__fStdOut[a], s);
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

