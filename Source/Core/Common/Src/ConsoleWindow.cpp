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


// Includes
#include <string> // System: To be able to add strings with "+"
#include <stdio.h>
#ifdef _WIN32
	#include <windows.h>
#else
#include <stdarg.h>
#endif

#include "Common.h"
#include "ConsoleWindow.h" // Common


// Declarations and definitions

namespace Console
{

// Create handles
	FILE* __fStdOut = NULL;
#ifdef _WIN32
	HANDLE __hStdOut = NULL;
#endif


/* Start console window - width and height is the size of console window, if you enable
   File the output will also be written to this file. */
void Open(int Width, int Height, char * Name, bool File)
{
#ifdef _WIN32
	// Open the console window and create the window handle for GetStdHandle()
	AllocConsole();

	// Save the window handle that AllocConsole() created
	__hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	// Set the console window title
	SetConsoleTitle(Name);

	// Set the total letter space
	COORD co = {Width, Height};
	SetConsoleScreenBufferSize(__hStdOut, co);

	/* Set the window size in number of letters. The height is hardcoded here because it can
	   be changed with MoveWindow() later */
	SMALL_RECT coo = {0,0, (Width - 1),50}; // Top, left, right, bottom
	SetConsoleWindowInfo(__hStdOut, TRUE, &coo);


#endif
	// Create a file and a file handle if File is enabled and we don't already have a file handle
	if(File && !__fStdOut)
	{
		// Edit the log file name
		std::string FileEnding = ".log";
		std::string FileName = Name;	
		std::string FullFilename = (FileName + FileEnding);

		// Open the file handle
		__fStdOut = fopen(FullFilename.c_str(), "w");
	}

}


/* Close the console window and close the eventual file handle */
void Close()
{
#ifdef _WIN32
	FreeConsole(); // Close the console window
#endif
	if(__fStdOut) fclose(__fStdOut); // Close the file handle

}


// Print to screen and file
int Print(const char *fmt, ...)
{
	// Maximum bytes, mind this value to avoid an overrun
	static const int MAX_BYTES = 1024*20;

#if defined(_WIN32)
	if(__hStdOut)
	{
#endif
		char s[MAX_BYTES];
		va_list argptr;
		int cnt; // To store the vsnprintf return message

		va_start(argptr, fmt);
		cnt = vsnprintf(s, MAX_BYTES, fmt, argptr);
		va_end(argptr);


#if defined(_WIN32)
		DWORD cCharsWritten; // We will get a value back here

		WriteConsole(__hStdOut, s, (DWORD)strlen(s), &cCharsWritten, NULL);
#else
		fprintf(stderr, "%s", s);
#endif
		// Write to the file
		if(__fStdOut)
		{
		    fprintf(__fStdOut, "%s", s);
		    fflush(__fStdOut); // Write file now, don't wait
		}

		return(cnt);
	
#if defined(_WIN32) 
	} else
	{
		return 0;
	}
#endif

}


// Clear console screen
void ClearScreen() 
{ 
#if defined(_WIN32)
	if(__hStdOut) // Check that we have a window handle
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


/* Get window handle of console window to be able to resize it. We use
   GetConsoleTitle() and FindWindow() to locate the console window handle. */
#if defined(_WIN32)
HWND GetHwnd(void)
{
   #define MY_BUFSIZE 1024 // Buffer size for console window titles
   HWND hwndFound;         // This is what is returned to the caller
   char pszNewWindowTitle[MY_BUFSIZE]; // Contains fabricated WindowTitle
   char pszOldWindowTitle[MY_BUFSIZE]; // Contains original WindowTitle

   // Fetch current window title.
   GetConsoleTitle(pszOldWindowTitle, MY_BUFSIZE);

   // Format a "unique" NewWindowTitle
   wsprintf(pszNewWindowTitle, "%d/%d", GetTickCount(), GetCurrentProcessId());

   // Change current window title
   SetConsoleTitle(pszNewWindowTitle);

   // Ensure window title has been updated
   Sleep(40);

   // Look for NewWindowTitle
   hwndFound = FindWindow(NULL, pszNewWindowTitle);

   // Restore original window title
   SetConsoleTitle(pszOldWindowTitle);

   return(hwndFound);
}
#endif // _WIN32

} // namespace
