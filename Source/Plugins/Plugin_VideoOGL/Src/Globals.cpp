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

#if !defined(OSX64)
#include <wx/wx.h>
#include <wx/filepicker.h>
#include <wx/notebook.h>
#include <wx/dialog.h>
#include <wx/aboutdlg.h>
#endif

#include "Globals.h"

#include "pluginspecs_video.h"
#include "main.h"

#include "IniFile.h"
#include <assert.h>

#ifdef _WIN32

// The one for Linux is in Linux/Linux.cpp
static HANDLE hConsole = NULL;

void OpenConsole()
{
	COORD csize;
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo; 
	SMALL_RECT srect;

	if (hConsole)
		return;
	AllocConsole();
	SetConsoleTitle("Opengl Plugin Output");

	// set width and height
	csize.X = 155; // this fits on 1280 pixels TODO: make it adjustable from the wx debugging window
	csize.Y = 1024;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), csize);

	// make the internal buffer match the width we set
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbiInfo);
	srect = csbiInfo.srWindow;
	srect.Right = srect.Left + csize.X - 1; // match
	srect.Bottom = srect.Top + 44;
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &srect);

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
}

void CloseConsole()
{
	if (hConsole == NULL)
		return;
	FreeConsole();
	hConsole = NULL;
}
#endif

static FILE* pfLog = NULL;
void __Log(const char *fmt, ...)
{
    char* Msg = (char*)alloca(strlen(fmt)+512);
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( Msg, strlen(fmt)+512, fmt, ap );
    va_end( ap );

    g_VideoInitialize.pLog(Msg, FALSE);

    if (pfLog == NULL)
		pfLog = fopen(FULL_LOGS_DIR "oglgfx.txt", "w");

    if (pfLog != NULL)
        fwrite(Msg, strlen(Msg), 1, pfLog);
#ifdef _WIN32
    DWORD tmp;
    WriteConsole(hConsole, Msg, (DWORD)strlen(Msg), &tmp, 0);
#else
	//printf("%s", Msg);
#endif
}

void __Log(int type, const char *fmt, ...)
{
    char* Msg = (char*)alloca(strlen(fmt)+512);
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( Msg, strlen(fmt)+512, fmt, ap );
    va_end( ap );

    g_VideoInitialize.pLog(Msg, FALSE);

#ifdef _WIN32
    DWORD tmp;
    WriteConsole(hConsole, Msg, (DWORD)strlen(Msg), &tmp, 0);
#endif
}
