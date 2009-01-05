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
// Include
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//#include <iostream>
#include <string>
//#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
//#include <tchar.h>
/////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯

// Enable or disable logging to screen and file
#define MM_DEBUG
//#define MM_DEBUG_FILEONLY

#ifdef MM_DEBUG
    FILE* __fStdOut = NULL;
#endif
#ifndef MM_DEBUG_FILEONLY
	HANDLE __hStdOut = NULL;
#endif

/////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////
// Start console window
/* ¯¯¯¯¯¯¯¯¯¯
   Width and height is the size of console window, if you specify fname, the output will
   also be writton to this file. The file pointer is automatically closed
   when you close the application. */
// ---------------------
void StartConsoleWin(int width, int height, char* fname)
{
#ifdef MM_DEBUG

	#ifndef MM_DEBUG_FILEONLY
		// Allocate console
		AllocConsole();

		// Set console window title
		SetConsoleTitle(fname);

		// Get window handle to write to
		__hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

		// Create coordinates table
		COORD co = {width, height};

		// Set the innteral letter space
		SetConsoleScreenBufferSize(__hStdOut, co);

		// Set the window width and height
		SMALL_RECT coo = {0,0, (width - 1),50}; // Top, left, right, bottom
		SetConsoleWindowInfo(__hStdOut, true, &coo);
	#endif

	if(fname)
	{
		// Edit the log file name
		std::string FileEnding = ".log";
		std::string FileName = fname;
		std::string FullFilename = (FileName + FileEnding);
		__fStdOut = fopen(FullFilename.c_str(), "w");
	}

#endif
}
/////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Printf function
// ¯¯¯¯¯¯¯¯¯¯
int wprintf(char *fmt, ...)
{
	#ifdef MM_DEBUG
		char s[500]; // Bigget message size
		va_list argptr;
		int cnt;

		va_start(argptr, fmt);
		cnt = vsprintf(s, fmt, argptr);
		va_end(argptr);

		DWORD cCharsWritten;

		// ---------------------------------------------------
		// Write to console
		// --------------
		#ifndef MM_DEBUG_FILEONLY
			if(__hStdOut)
				WriteConsole(__hStdOut, s, strlen(s), &cCharsWritten, NULL);
		#endif
		// ----------------------------

		// Write to file
		if(__fStdOut)
		{
			fprintf(__fStdOut, s);
			//fflush(__fStdOut); // Write file now, don't wait
		}

		return(cnt);
	#else
		return 0;
	#endif
}
/////////////////////////////