// Copyright (C) 2003-2009 Dolphin Project.

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

#include <algorithm>  // min
#include <string> // System: To be able to add strings with "+"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <stdarg.h>
#endif

#include "Common.h"
#include "LogManager.h" // Common


ConsoleListener::ConsoleListener()
{
#ifdef _WIN32
	m_hStdOut = NULL;
#endif
}

ConsoleListener::~ConsoleListener()
{
	Close();
}

// 100, 100, "Dolphin Log Console"
// Open console window - width and height is the size of console window
// Name is the window title
void ConsoleListener::Open(int width, int height, char *title)
{
#ifdef _WIN32
	// Open the console window and create the window handle for GetStdHandle()
	AllocConsole();

	// Save the window handle that AllocConsole() created
	m_hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	// Set the console window title
	SetConsoleTitle(title);

	// Set the total letter space
	COORD co = {width, 3000};  // Big scrollback.
	SetConsoleScreenBufferSize(m_hStdOut, co);

	/* Set the window size in number of letters. The height is hard coded here
	because it can be changed with MoveWindow() later */
	SMALL_RECT coo = {0,0, (width - 1), 50}; // Top, left, right, bottom
	SetConsoleWindowInfo(m_hStdOut, TRUE, &coo);
#endif
}

// Close the console window and close the eventual file handle
void ConsoleListener::Close()
{
#ifdef _WIN32
	if (m_hStdOut == NULL)
		return;
	FreeConsole();
	m_hStdOut = NULL;
#else
	fflush(NULL);
#endif
}

bool ConsoleListener::IsOpen()
{
#ifdef _WIN32
	return (m_hStdOut != NULL);
#else
	return true;
#endif
}

// Logs the message to screen
void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char *text) 
{
#if defined(_WIN32)
	DWORD cCharsWritten; // We will get a value back here
	WORD color;
	
	switch (level)
	{
	case NOTICE_LEVEL: // light green
		color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		break;

	case ERROR_LEVEL: // light red
		color = FOREGROUND_RED | FOREGROUND_INTENSITY;
		break;

	case WARNING_LEVEL: // light yellow
		color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		break;

	case INFO_LEVEL: // cyan
		color = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		break;

	case DEBUG_LEVEL: // gray
		color = FOREGROUND_INTENSITY;
		break;

	default: // off-white
		color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		break;
	}
	if (strlen(text) > 10) {
		// first 10 chars white
		SetConsoleTextAttribute(m_hStdOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		WriteConsole(m_hStdOut, text, 10, &cCharsWritten, NULL);
		text += 10;
	}
	SetConsoleTextAttribute(m_hStdOut, color);
	WriteConsole(m_hStdOut, text, (DWORD)strlen(text), &cCharsWritten, NULL);
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
