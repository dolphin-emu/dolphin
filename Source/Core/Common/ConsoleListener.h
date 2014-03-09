// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/LogManager.h"

#ifdef _WIN32
#include <windows.h>
#endif

class ConsoleListener : public LogListener
{
public:
	ConsoleListener();
	~ConsoleListener();

	void Open(bool Hidden = false, int Width = 100, int Height = 100, const char * Name = "Console");
	void UpdateHandle();
	void Close();
	bool IsOpen();
	void LetterSpace(int Width, int Height);
	void BufferWidthHeight(int BufferWidth, int BufferHeight, int ScreenWidth, int ScreenHeight, bool BufferFirst);
	void PixelSpace(int Left, int Top, int Width, int Height, bool);
#ifdef _WIN32
	COORD GetCoordinates(int BytesRead, int BufferWidth);
#endif
	void Log(LogTypes::LOG_LEVELS, const char *Text) override;
	void ClearScreen(bool Cursor = true);

private:
#ifdef _WIN32
	HWND GetHwnd(void);
	HANDLE hConsole;
#endif
	bool bUseColor;
};
