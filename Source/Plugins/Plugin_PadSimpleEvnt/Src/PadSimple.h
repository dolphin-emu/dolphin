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

#ifndef __PADSIMPLE_H__
#define __PADSIMPLE_H__  

#include "EventHandler.h"
// Controls
enum
{
	CTL_A = 0,
	CTL_B,
	CTL_X,
	CTL_Y,
	CTL_Z,
	CTL_START,
	CTL_L,
	CTL_R,
	CTL_MAINUP,
	CTL_MAINDOWN,
	CTL_MAINLEFT,
	CTL_MAINRIGHT,
	CTL_SUBUP,
	CTL_SUBDOWN,
	CTL_SUBLEFT,
	CTL_SUBRIGHT,
	CTL_DPADUP,
	CTL_DPADDOWN,
	CTL_DPADLEFT,
	CTL_DPADRIGHT,
	CTL_HALFPRESS,
	CTL_MIC,
	NUMCONTROLS
};


struct SPads {
	bool bAttached;		// Pad is "attached" to the gamecube/wii
	bool bDisable;		// Disabled when dolphin isn't in focus
	unsigned int keyForControl[NUMCONTROLS];// Keyboard mapping
};

extern SPads pad[];

void LoadConfig();
void SaveConfig();

#endif
