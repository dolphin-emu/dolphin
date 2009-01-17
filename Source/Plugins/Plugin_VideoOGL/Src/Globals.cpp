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
// Includes
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
#if defined(HAVE_WX) && HAVE_WX
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
#include "ConsoleWindow.h"
#include <assert.h>
/////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Open and close the Windows console window
// ¯¯¯¯¯¯¯¯¯¯¯¯¯
#ifdef _WIN32
void OpenConsole()
{
	Console::Open(100, 300, "OpenGL Plugin Output"); // give room for 300 rows
	Console::Print("OpenGL console opened\n");
	MoveWindow(Console::GetHwnd(), 0,400, 1280,550, true); // Move window. Todo: make this
			// adjustable from the debugging window
}

void CloseConsole()
{
	Console::Close();
}
#endif
//////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Write logs
// ¯¯¯¯¯¯¯¯¯¯¯¯¯

// The log file handle
static FILE* pfLog = NULL;

// This is on by default, but can be controlled from the debugging window
bool LocalLogFile = true;

void __Log(const char *fmt, ...)
{
    char* Msg = (char*)alloca(strlen(fmt)+512);
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( Msg, strlen(fmt)+512, fmt, ap );
    va_end( ap );

    g_VideoInitialize.pLog(Msg, FALSE);

	// If we have no file to write to, create one
    if (pfLog == NULL && LocalLogFile)
		pfLog = fopen(FULL_LOGS_DIR "oglgfx.txt", "w");

	// Write to file
    if (pfLog != NULL && LocalLogFile)
        fwrite(Msg, strlen(Msg), 1, pfLog);

	#ifdef _WIN32
		// Write to the console screen, if one exists
		Console::Print(Msg);
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
	// Write to the console screen, if one exists
    Console::Print(Msg);
#endif
}
//////////////////////////////////
