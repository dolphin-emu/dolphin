// Copyright (C) 2003 Dolphin Project.

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


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include <algorithm>  // min
#include <string> // System: To be able to add strings with "+"
#include <stdio.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <stdarg.h>
#endif

#include "Common.h"
#include "LogManager.h" // Common
////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
ConsoleListener::ConsoleListener()
{
#ifdef _WIN32
	hConsole = NULL;
#endif
}

ConsoleListener::~ConsoleListener()
{
	Close();
}

// 100, 100, "Dolphin Log Console"
// Open console window - width and height is the size of console window
// Name is the window title
void ConsoleListener::Open(int Width, int Height, const char *Title)
{
#ifdef _WIN32
	// Open the console window and create the window handle for GetStdHandle()
	AllocConsole();

	// Save the window handle that AllocConsole() created
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// Set the console window title
	SetConsoleTitle(Title);

	// Set the total letter space to MAX_BYTES
	int LWidth = Width / 8;
	int LBufHeight = floor((float)(MAX_BYTES / (LWidth + 1)));
	COORD co = {LWidth, LBufHeight};
	SetConsoleScreenBufferSize(hConsole, co);

	/* Set the window size in number of letters. The height is hard coded here
	because it can be changed with MoveWindow() later */
	SMALL_RECT coo = {0,0, (LWidth - 1), 50}; // Top, left, right, bottom
	SetConsoleWindowInfo(hConsole, TRUE, &coo);
#endif
}

// Close the console window and close the eventual file handle
void ConsoleListener::Close()
{
#ifdef _WIN32
	if (hConsole == NULL)
		return;
	FreeConsole();
	hConsole = NULL;
#else
	fflush(NULL);
#endif
}

bool ConsoleListener::IsOpen()
{
#ifdef _WIN32
	return (hConsole != NULL);
#else
	return true;
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Size
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ConsoleListener::LetterSpace(int Width, int Height)
{
#ifdef _WIN32

	// Get console info
	CONSOLE_SCREEN_BUFFER_INFO ConInfo;
	GetConsoleScreenBufferInfo(hConsole, &ConInfo);

	// Change the screen buffer window size
	SMALL_RECT coo = {0,0, Width, Height}; // top, left, right, bottom
	bool SW = SetConsoleWindowInfo(hConsole, TRUE, &coo);

	// Change screen buffer to the screen buffer window size
	COORD Co = {Width + 1, Height + 1};
	bool SB = SetConsoleScreenBufferSize(hConsole, Co);

	// Resize the window too
	MoveWindow(GetConsoleWindow(), 200,200, (Width*8 + 50),(Height*12 + 50), true);

	// Logging
	//printf("LetterSpace(): L:%i T:%i Iconic:%i\n", ConsoleLeft, ConsoleTop, ConsoleIconic);
#endif
}
void ConsoleListener::PixelSpace(int Left, int Top, int Width, int Height, bool Resize)
{
#ifdef _WIN32
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// Get console info
	CONSOLE_SCREEN_BUFFER_INFO ConInfo;
	GetConsoleScreenBufferInfo(hConsole, &ConInfo);
	// Letter space
	int LWidth = floor((float)(Width / 8));
	int LHeight = floor((float)(Height / 12));	
	int LBufHeight = floor((float)(MAX_BYTES / (LWidth + 1)));

	// Read the current text
	char Str[MAX_BYTES];
	DWORD dwConSize = ConInfo.dwSize.X * ConInfo.dwSize.Y;
	DWORD cCharsRead = 0;
	COORD coordScreen = { 0, 0 };
	ReadConsoleOutputCharacter(hConsole, Str, dwConSize, coordScreen, &cCharsRead);

	// Change the screen buffer window size
	SMALL_RECT coo = {0,0, LWidth, LHeight}; // top, left, right, bottom
	bool bW = SetConsoleWindowInfo(hConsole, TRUE, &coo);
	// Change screen buffer to the screen buffer window size
	COORD Co = {LWidth + 1, LBufHeight};
	bool bB = SetConsoleScreenBufferSize(hConsole, Co);

	// Redraw the text
	DWORD cCharsWritten = 0;
	WriteConsoleOutputCharacter(hConsole, Str, cCharsRead, coordScreen, &cCharsWritten);

	// Resize the window too
	if (Resize) MoveWindow(GetConsoleWindow(), Left,Top, (Width + 50),(Height + 50), true);

	// Logging
	//Log(LogTypes::LNOTICE, StringFromFormat("\n\n\n\nMaxBytes: %i, WxH:%ix%i, Consize: %i, Read: %i, Written: %i\n\n", MAX_BYTES, LWidth, LHeight, dwConSize, cCharsRead, cCharsWritten).c_str());
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////////
// Write
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//void ConsoleListener::Log(LogTypes::LOG_LEVELS Level, const char *Text, ...)
void ConsoleListener::Log(LogTypes::LOG_LEVELS Level, const char *Text)
{
#if defined(_WIN32)
	/*
	const int MAX_BYTES = 1024*10;
	char Str[MAX_BYTES];
	va_list ArgPtr;
	int Cnt;
	va_start(ArgPtr, Text);
	Cnt = vsnprintf(Str, MAX_BYTES, Text, ArgPtr);
	va_end(ArgPtr);
	*/
	DWORD cCharsWritten;
	WORD Color;
	
	switch (Level)
	{
	case NOTICE_LEVEL: // light green
		Color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		break;
	case ERROR_LEVEL: // light red
		Color = FOREGROUND_RED | FOREGROUND_INTENSITY;
		break;
	case WARNING_LEVEL: // light yellow
		Color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
		break;
	case INFO_LEVEL: // cyan
		Color = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
		break;
	case DEBUG_LEVEL: // gray
		Color = FOREGROUND_INTENSITY;
		break;
	default: // off-white
		Color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		break;
	}
	if (Level != CUSTOM_LEVEL && strlen(Text) > 10)
	{
		// First 10 chars white
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		WriteConsole(hConsole, Text, 10, &cCharsWritten, NULL);
		Text += 10;
	}
	SetConsoleTextAttribute(hConsole, Color);
	WriteConsole(hConsole, Text, (DWORD)strlen(Text), &cCharsWritten, NULL);
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
	// Write space to the entire console
	FillConsoleOutputCharacter(hConsole, TEXT(' '), dwConSize, coordScreen, &cCharsWritten); 
	GetConsoleScreenBufferInfo(hConsole, &csbi); 
	FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
	// Reset cursor
	SetConsoleCursorPosition(hConsole, coordScreen); 
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////////

