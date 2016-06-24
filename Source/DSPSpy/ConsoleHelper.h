// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <iostream>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define CON_BLACK 0
#define CON_RED 1
#define CON_GREEN 2
#define CON_YELLOW 3
#define CON_BLUE 4
#define CON_MAGENTA 5
#define CON_CYAN 6
#define CON_WHITE 7
#define CON_BRIGHT 8
#define CON_BRIGHT_BLACK CON_BLACK | CON_BRIGHT
#define CON_BRIGHT_RED CON_RED | CON_BRIGHT
#define CON_BRIGHT_GREEN CON_GREEN | CON_BRIGHT
#define CON_BRIGHT_YELLOW CON_YELLOW | CON_BRIGHT
#define CON_BRIGHT_BLUE CON_BLUE | CON_BRIGHT
#define CON_BRIGHT_MAGENTA CON_MAGENTA | CON_BRIGHT
#define CON_BRIGHT_CYAN CON_CYAN | CON_BRIGHT
#define CON_BRIGHT_WHITE CON_WHITE | CON_BRIGHT

inline void CON_Printf(const int x, const int y, const char* fmt, ...)
{
  char tmpbuf[255];

  va_list marker;
  va_start(marker, fmt);
  vsprintf(tmpbuf, fmt, marker);
  va_end(marker);

  printf("\x1b[%d;%dH%s", y, x, tmpbuf);
}

inline void CON_SetColor(u8 foreground, u8 background = CON_BLACK)
{
  u8 bright = foreground & CON_BRIGHT ? 1 : 0;

  if (bright)
    foreground &= ~CON_BRIGHT;

  printf("\x1b[%d;%d;%dm", 30 + foreground, bright, 40 + background);
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
  char blank[columns];
  std::fill(blank, blank + columns, ' ');
  blank[columns - 1] = '\0';
  CON_Printf(0, y, "%s", blank);
}

#define CON_PrintRow(x, y, ...)                                                                    \
  {                                                                                                \
    CON_BlankRow(y);                                                                               \
    CON_Printf(x, y, __VA_ARGS__);                                                                 \
  }
