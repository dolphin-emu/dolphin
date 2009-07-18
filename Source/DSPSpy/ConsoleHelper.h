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

#ifndef _CONSOLEHELPER_H_
#define _CONSOLEHELPER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <iostream>

#define CON_BLACK 		0
#define CON_RED			1
#define CON_GREEN		2
#define CON_YELLOW		3
#define CON_BLUE		4
#define CON_MAGENTA		5
#define CON_CYAN		6
#define	CON_WHITE		7
#define CON_BRIGHT		8
#define CON_BRIGHT_BLACK 		CON_BLACK	| CON_BRIGHT
#define CON_BRIGHT_RED			CON_RED		| CON_BRIGHT
#define CON_BRIGHT_GREEN		CON_GREEN	| CON_BRIGHT
#define CON_BRIGHT_YELLOW		CON_YELLOW	| CON_BRIGHT
#define CON_BRIGHT_BLUE			CON_BLUE	| CON_BRIGHT
#define CON_BRIGHT_MAGENTA		CON_MAGENTA	| CON_BRIGHT
#define CON_BRIGHT_CYAN			CON_CYAN	| CON_BRIGHT
#define	CON_BRIGHT_WHITE		CON_WHITE	| CON_BRIGHT


inline void CON_Printf(const int x, const int y, const char* fmt, ...)
{
	char tmpbuf[255];

	va_list marker;
	va_start(marker,fmt);
	vsprintf(tmpbuf, fmt, marker);
	va_end(marker);

	printf("\x1b[%d;%dH%s", y, x, tmpbuf);
}

inline void CON_SetColor(u8 foreground, u8 background = CON_BLACK)
{
	u8 bright = foreground & CON_BRIGHT ? 1 : 0;

	if (bright)
		foreground &= ~CON_BRIGHT;

	printf("\x1b[%d;%d;%dm", 30+foreground, bright, 40+background);
}

inline void CON_Clear()
{
	// Escape code to clear the whole screen.
	printf("\x1b[2J");
}

// libogc's clear escape codes are crappy
inline void CON_BlankRow(const int y)
{
	int columns = 0, rows = 0;
	CON_GetMetrics(&columns, &rows);
	char* blank = new char[columns];
	std::fill(blank, &blank[columns], ' ');
	CON_Printf(0, y, "%s", blank);
	delete blank;
}

#define CON_PrintRow(x, y, ...) \
{ \
	CON_BlankRow(y); \
	CON_Printf(x, y, __VA_ARGS__); \
}

#endif
