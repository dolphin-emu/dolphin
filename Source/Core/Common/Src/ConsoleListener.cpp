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


#include <string> // System: To be able to add strings with "+"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <stdarg.h>
#endif

#include "Common.h"
#include "LogManager.h" // Common


/* Start console window - width and height is the size of console window */
ConsoleListener::ConsoleListener(int Width, int Height, char * Name) :
	Listener("console")
{
#ifdef _WIN32
	// Open the console window and create the window handle for GetStdHandle()
	AllocConsole();

	// Save the window handle that AllocConsole() created
	m_hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	// Set the console window title
	SetConsoleTitle(Name);

	// Set the total letter space
	COORD co = {Width, Height};
	SetConsoleScreenBufferSize(m_hStdOut, co);

	/* Set the window size in number of letters. The height is hardcoded here
	   because it can be changed with MoveWindow() later */
	SMALL_RECT coo = {0,0, (Width - 1),50}; // Top, left, right, bottom
	SetConsoleWindowInfo(m_hStdOut, TRUE, &coo);
#endif

}


/* Close the console window and close the eventual file handle */
ConsoleListener::~ConsoleListener()
{
#ifdef _WIN32
	FreeConsole(); // Close the console window
#else
	fflush(NULL);
#endif
}

// Logs the message to screen
void ConsoleListener::Log(LogTypes::LOG_LEVELS, const char *text) 
{
	
#if defined(_WIN32)
		DWORD cCharsWritten; // We will get a value back here

		WriteConsole(m_hStdOut, text, (DWORD)strlen(text), 
					 &cCharsWritten, NULL);
#else
		fprintf(stderr, "%s", text);
#endif
}


// Clear console screen
void ConsoleListener::ClearScreen() 
{ 
#if defined(_WIN32)
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

