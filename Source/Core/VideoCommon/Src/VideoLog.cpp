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

#include "VideoCommon.h"
#include <stdio.h>
#include <malloc.h>

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
//    DWORD tmp;
//    WriteConsole(hConsole, Msg, (DWORD)strlen(Msg), &tmp, 0);
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
//    DWORD tmp;
  //  WriteConsole(hConsole, Msg, (DWORD)strlen(Msg), &tmp, 0);
#endif
}
