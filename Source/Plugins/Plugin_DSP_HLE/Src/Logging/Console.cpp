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



#include <string>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif



// On and off
bool g_consoleEnable = true;
#define DEBUGG
//#define DEBUGG_FILEONLY
//#define DEBUGG_NOFILE
// --------------------


// Create handles

#ifdef DEBUGG
FILE* __fStdOut = NULL;
#endif
#ifndef DEBUGG_FILEONLY
#ifdef _WIN32
HANDLE __hStdOut = NULL;
#endif
#endif
// --------------------


// =======================================================================================
/* Start console window - width and height is the size of console window, if you specify
fname, the output will also be writton to this file. The file pointer is automatically
closed when you close the app */
// ---------------------------------------------------------------------------------------
void startConsoleWin(int width, int height, char* fname)
{

#if defined(DEBUGG) && defined(_WIN32)

#ifndef DEBUGG_FILEONLY

	AllocConsole();

	SetConsoleTitle(fname);
	__hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	COORD co = {width,height};
	SetConsoleScreenBufferSize(__hStdOut, co);

	SMALL_RECT coo = {0,0,(width - 1),70}; // top, left, right, bottom
	SetConsoleWindowInfo(__hStdOut, TRUE, &coo);

#endif
#ifndef DEBUGG_NOFILE
	// ---------------------------------------------------------------------------------------
	// Write to a file
	if(fname)
	{
		// Edit the log file name
		std::string FileEnding = ".log";
		std::string FileName = fname;
		std::string FullFilename = (FileName + FileEnding);
		__fStdOut = fopen(FullFilename.c_str(), "w");
	}
	// ---------------------------------------------------------------------------------------
#endif

#endif
}


// ---------------------------------------------------------------------------------------
// Printf function
int wprintf(char *fmt, ...)
{
#if defined(DEBUGG) && defined(_WIN32)
	char s[3000]; // WARNING: Mind this value
	va_list argptr;
	int cnt;

	va_start(argptr, fmt);
	cnt = vsnprintf(s, 3000, fmt, argptr);
	va_end(argptr);

	DWORD cCharsWritten;

	// ---------------------------------------------------------------------------------------
#ifndef DEBUGG_FILEONLY
	if(__hStdOut)
	{
		WriteConsole(__hStdOut, s, strlen(s), &cCharsWritten, NULL);
	}
#endif
#ifndef DEBUGG_NOFILE
	// ---------------------------------------------------------------------------------------
	
	if(__fStdOut)
		fprintf(__fStdOut, s);
	// ---------------------------------------------------------------------------------------
#endif

	return(cnt);
#else
	return 0;
#endif
}


void ClearScreen() 
{ 
#if defined(DEBUGG) && defined(_WIN32)
	if(g_consoleEnable)
	{
		COORD coordScreen = { 0, 0 }; 
		DWORD cCharsWritten; 
		CONSOLE_SCREEN_BUFFER_INFO csbi; 
		DWORD dwConSize; 

		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); 
	    
		GetConsoleScreenBufferInfo(hConsole, &csbi); 
		dwConSize = csbi.dwSize.X * csbi.dwSize.Y; 
		FillConsoleOutputCharacter(hConsole, TEXT(' '), dwConSize, 
			coordScreen, &cCharsWritten); 
		GetConsoleScreenBufferInfo(hConsole, &csbi); 
		FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, 
			coordScreen, &cCharsWritten); 
		SetConsoleCursorPosition(hConsole, coordScreen); 
	}
#endif
}

#if defined(DEBUGG) && defined(_WIN32)
HWND GetConsoleHwnd(void)
{

   #define MY_BUFSIZE 1024 // Buffer size for console window titles.
   HWND hwndFound;         // This is what is returned to the caller.
   char pszNewWindowTitle[MY_BUFSIZE]; // Contains fabricated
                                       // WindowTitle.
   char pszOldWindowTitle[MY_BUFSIZE]; // Contains original
                                       // WindowTitle.

   // Fetch current window title.

   GetConsoleTitle(pszOldWindowTitle, MY_BUFSIZE);

   // Format a "unique" NewWindowTitle.

   wsprintf(pszNewWindowTitle,"%d/%d",
               GetTickCount(),
               GetCurrentProcessId());

   // Change current window title.

   SetConsoleTitle(pszNewWindowTitle);

   // Ensure window title has been updated.

   Sleep(40);

   // Look for NewWindowTitle.

   hwndFound = FindWindow(NULL, pszNewWindowTitle);

   // Restore original window title.

   SetConsoleTitle(pszOldWindowTitle);

   return(hwndFound);

}
#endif // win32
