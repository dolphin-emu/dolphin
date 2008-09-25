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

// controls
#ifndef __PADSIMPLE_H__
#define __PADSIMPLE_H__  
enum
{
	CTL_A = 0,
	CTL_B,
	CTL_X,
	CTL_Y,
	CTL_Z,
	CTL_START,
	CTL_HALFTRIGGER,
	CTL_L,
	CTL_R,
	CTL_MAINUP,
	CTL_MAINDOWN,
	CTL_MAINLEFT,
	CTL_MAINRIGHT,
	CTL_HALFMAIN,
	CTL_SUBUP,
	CTL_SUBDOWN,
	CTL_SUBLEFT,
	CTL_SUBRIGHT,
	CTL_HALFSUB,
	CTL_DPADUP,
	CTL_DPADDOWN,
	CTL_DPADLEFT,
	CTL_DPADRIGHT,
	NUMCONTROLS
};

// control names
static const char* controlNames[] =
{
	"A_button",
	"B_button",
	"X_button",
	"Y_button",
	"Z_trigger",
	"Start",
	"Soft_trigger_switch",
	"L_button",
	"R_button",
	"Main_stick_up",
	"Main_stick_down",
	"Main_stick_left",
	"Main_stick_right",
	"Soft_main_switch",
	"Sub_stick_up",
	"Sub_stick_down",
	"Sub_stick_left",
	"Sub_stick_right",
	"Soft_sub_switch",
	"D-Pad_up",
	"D-Pad_down",
	"D-Pad_left",
	"D-Pad_right",
};

// control human readable
static const char* userControlNames[] =
{
	"A",
	"B",
	"X",
	"Y",
	"Z",
	"Start",
	"Analog", // Don't use
	"L",
	"R",
	"Up",
	"Down",
	"Left",
	"Right",
	"Soft_main_switch", // Don't use
	"Up",
	"Down",
	"Left",
	"Right",
	"Soft_sub_switch", // Don't use
	"Up",
	"Down",
	"Left",
	"Right",
};

struct SPads {
	bool type;		//keyboard = 0, xpad = 1
	int XPad;		//player# of the xpad
	bool attached;	//pad is "attached" to the gamecube/wii
	bool disable;	//disabled when dolphin isn't in focus
	bool rumble;	//rumble for xpad
	unsigned int keyForControl[NUMCONTROLS];//keyboard mapping
};

extern SPads pad[];

void LoadConfig();
void SaveConfig();

#endif
