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
	// Set letter space
	LetterSpace(80, 4000);
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

/*
  LetterSpace: SetConsoleScreenBufferSize and SetConsoleWindowInfo are
	dependent on each other, that's the reason for the additional checks.  
*/
void ConsoleListener::BufferWidthHeight(int BufferWidth, int BufferHeight, int ScreenWidth, int ScreenHeight, bool BufferFirst)
{
#ifdef _WIN32
	bool SB, SW;

	if (BufferFirst)
	{
		// Change screen buffer size
		COORD Co = {BufferWidth, BufferHeight};
		SB = SetConsoleScreenBufferSize(hConsole, Co);
		// Change the screen buffer window size
		SMALL_RECT coo = {0,0,ScreenWidth, ScreenHeight}; // top, left, right, bottom
		SW = SetConsoleWindowInfo(hConsole, TRUE, &coo);
	}
	else
	{
		// Change the screen buffer window size
		SMALL_RECT coo = {0,0, ScreenWidth, ScreenHeight}; // top, left, right, bottom
		SW = SetConsoleWindowInfo(hConsole, TRUE, &coo);
		// Change screen buffer size
		COORD Co = {BufferWidth, BufferHeight};
		SB = SetConsoleScreenBufferSize(hConsole, Co);
	}
#endif
}
void ConsoleListener::LetterSpace(int Width, int Height)
{
#ifdef _WIN32
	// Get console info
	CONSOLE_SCREEN_BUFFER_INFO ConInfo;
	GetConsoleScreenBufferInfo(hConsole, &ConInfo);

	//
	int OldBufferWidth = ConInfo.dwSize.X;
	int OldBufferHeight = ConInfo.dwSize.Y;
	int OldScreenWidth = (ConInfo.srWindow.Right - ConInfo.srWindow.Left);
	int OldScreenHeight = (ConInfo.srWindow.Bottom - ConInfo.srWindow.Top);
	//
	int NewBufferWidth = Width;
	int NewBufferHeight = Height;
	int NewScreenWidth = NewBufferWidth - 1;
	int NewScreenHeight = OldScreenHeight;

	// Width
	BufferWidthHeight(NewBufferWidth, OldBufferHeight, NewScreenWidth, OldScreenHeight, (NewBufferWidth > OldScreenWidth-1));
	// Height
	BufferWidthHeight(NewBufferWidth, NewBufferHeight, NewScreenWidth, NewScreenHeight, (NewBufferHeight > OldScreenHeight-1));

	// Resize the window too
	//MoveWindow(GetConsoleWindow(), 200,200, (Width*8 + 50),(NewScreenHeight*12 + 200), true);
#endif
}
#ifdef _WIN32
COORD ConsoleListener::GetCoordinates(int BytesRead, int BufferWidth)
{
	COORD Ret = {0, 0};
	// Full rows
	int Step = floor((float)BytesRead / (float)BufferWidth);
	Ret.Y += Step;
	// Partial row
	Ret.X = BytesRead - (BufferWidth * Step);
	return Ret;
}
#endif
void ConsoleListener::PixelSpace(int Left, int Top, int Width, int Height, bool Resize)
{
#ifdef _WIN32
	// Check size
	if (Width < 8 || Height < 12) return;

	bool DBef = true;
	bool DAft = true;
	std::string SLog = "";

	HWND hWnd = GetConsoleWindow();
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// Get console info
	CONSOLE_SCREEN_BUFFER_INFO ConInfo;
	GetConsoleScreenBufferInfo(hConsole, &ConInfo);
	DWORD BufferSize = ConInfo.dwSize.X * ConInfo.dwSize.Y;

	// ---------------------------------------------------------------------
	//  Save the current text
	// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
	DWORD cCharsRead = 0;
	COORD coordScreen = { 0, 0 };

	std::vector<char*> Str;
	std::vector<WORD*> Attr;
	// ReadConsoleOutputAttribute seems to have a limit at this level
	const int MAX_BYTES = 1024 * 16;
	int ReadBufferSize = MAX_BYTES - 32;
	DWORD cAttrRead = ReadBufferSize;
	int BytesRead = 0;
	int i = 0;
	int LastAttrRead = 0;
	while(BytesRead < BufferSize)
	{
		Str.push_back(new char[MAX_BYTES]);
		if (!ReadConsoleOutputCharacter(hConsole, Str[i], ReadBufferSize, coordScreen, &cCharsRead))
			SLog += StringFromFormat("WriteConsoleOutputCharacter error");
		Attr.push_back(new WORD[MAX_BYTES]);
		if (!ReadConsoleOutputAttribute(hConsole, Attr[i], ReadBufferSize, coordScreen, &cAttrRead))
			SLog += StringFromFormat("WriteConsoleOutputAttribute error");

		// Break on error
		if (cAttrRead == 0) break;
		BytesRead += cAttrRead;
		i++;
		coordScreen = GetCoordinates(BytesRead, ConInfo.dwSize.X);
		LastAttrRead = cAttrRead;
	}
	// Letter space
	int LWidth = floor((float)Width / 8.0) - 1.0;
	int LHeight = floor((float)Height / 12.0) - 1.0;	
	int LBufWidth = LWidth + 1;
	int LBufHeight = floor((float)BufferSize / (float)LBufWidth);
	// Change screen buffer size
	LetterSpace(LBufWidth, LBufHeight);


	ClearScreen(true);	
	coordScreen.Y = 0;
	coordScreen.X = 0;
	DWORD cCharsWritten = 0;

	int BytesWritten = 0;
	DWORD cAttrWritten = 0;
	for (int i = 0; i < Attr.size(); i++)
	{
		if (!WriteConsoleOutputCharacter(hConsole, Str[i], ReadBufferSize, coordScreen, &cCharsWritten))
			SLog += StringFromFormat("WriteConsoleOutputCharacter error");
		if (!WriteConsoleOutputAttribute(hConsole, Attr[i], ReadBufferSize, coordScreen, &cAttrWritten))
			SLog += StringFromFormat("WriteConsoleOutputAttribute error");

		BytesWritten += cAttrWritten;
		coordScreen = GetCoordinates(BytesWritten, LBufWidth);
	}	

	int OldCursor = ConInfo.dwCursorPosition.Y * ConInfo.dwSize.X + ConInfo.dwCursorPosition.X;
	COORD Coo = GetCoordinates(OldCursor, LBufWidth);
	SetConsoleCursorPosition(hConsole, Coo);

	if (SLog.length() > 0) Log(LogTypes::LNOTICE, SLog.c_str());

	// Resize the window too
	if (Resize) MoveWindow(GetConsoleWindow(), Left,Top, (Width + 100),Height, true);
#endif
}

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
	fprintf(stderr, "%s", Text);
#endif
}
// Clear console screen
void ConsoleListener::ClearScreen(bool Cursor)
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
	if (Cursor) SetConsoleCursorPosition(hConsole, coordScreen); 
#endif
}


