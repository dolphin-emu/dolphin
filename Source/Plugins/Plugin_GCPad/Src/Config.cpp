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


#include <iostream>

#include "Common.h"
#include "IniFile.h"
#include "Config.h"
#include "GCPad.h"

static const char* gcControlNames[] =
{ 
	"Button_A",
	"Button_B",
	"Button_X",
	"Button_Y",
	"Button_Z",
	"Button_Start",

	"DPad_Up",
	"DPad_Down",
	"DPad_Left",
	"DPad_Right",

	"Stick_Up",
	"Stick_Down",
	"Stick_Left",
	"Stick_Right",

	"CStick_Up",
	"CStick_Down",
	"CStick_Left",
	"CStick_Right",

	"Shoulder_L",
	"Shoulder_R",
	"Shoulder_Semi_L",
	"Shoulder_Semi_R",
};

static const int gcDefaultControls[] =
#ifdef _WIN32
	{
		'X',
		'Z',
		'C',
		'S',
		'D',
		VK_RETURN,
		'T',
		'G',
		'F',
		'H',
		VK_UP,
		VK_DOWN,
		VK_LEFT,
		VK_RIGHT,
		'I',
		'K',
		'J',
		'L',
		'Q',
		'W',
		0x00,
		0x00,
	};
#elif defined(HAVE_X11) && HAVE_X11
	{
		XK_x, // A
		XK_z, // B
		XK_c, // X
		XK_s, // Y
		XK_d, // Z
		XK_Return, // Start
		XK_t, // D-pad up
		XK_g, // D-pad down
		XK_f, // D-pad left
		XK_h, // D-pad right
		XK_Up, // Main stick up
		XK_Down, // Main stick down
		XK_Left, // Main stick left
		XK_Right, // Main stick right
		XK_i, // C-stick up
		XK_k, // C-stick down
		XK_j, // C-stick left
		XK_l, // C-stick right
		XK_q, // L
		XK_w, // R
		0x00, // L semi-press
		0x00, // R semi-press
	};
#elif defined(HAVE_COCOA) && HAVE_COCOA
	// Reference for Cocoa key codes:
	// http://boredzo.org/blog/archives/2007-05-22/virtual-key-codes
	{
		7, // A (x)
		6, // B (z)
		8, // X (c)
		1, // Y (s)
		2, // Z (d)
		36, // Start (return)
		17, // D-pad up (t)
		5, // D-pad down (g)
		3, // D-pad left (f)
		4, // D-pad right (h)
		126, // Main stick up (up)
		125, // Main stick down (down)
		123, // Main stick left (left)
		124, // Main stick right (right)
		34, // C-stick up (i)
		40, // C-stick down (k)
		38, // C-stick left (j)
		37, // C-stick right (l)
		12, // L (q)
		13, // R (w)
		-1, // L semi-press (none)
		-1, // R semi-press (none)
	};
#endif


Config g_Config;

// Run when created
// -----------------
Config::Config()
{
}

// Save settings to file
// ---------------------
void Config::Save()
{
	// Load ini file
	IniFile file;
	file.Load(FULL_CONFIG_DIR "GCPad.ini");

	// ==================================================================
	// Global settings
	file.Set("General", "NoTriggerFilter", g_Config.bNoTriggerFilter);
#ifdef RERECORDING
	file.Set("General", "Recording", g_Config.bRecording);
	file.Set("General", "Playback", g_Config.bPlayback);
#endif

	for (int i = 0; i < 4; i++)
	{
		// ==================================================================
		// Slot specific settings only
		std::string SectionName = StringFromFormat("GCPad%i", i+1);
		file.Set(SectionName.c_str(), "DeviceID", GCMapping[i].ID);
		file.Set(SectionName.c_str(), "Axis_Lx", GCMapping[i].AxisMapping.Lx);
		file.Set(SectionName.c_str(), "Axis_Ly", GCMapping[i].AxisMapping.Ly);
		file.Set(SectionName.c_str(), "Axis_Rx", GCMapping[i].AxisMapping.Rx);
		file.Set(SectionName.c_str(), "Axis_Ry", GCMapping[i].AxisMapping.Ry);
		file.Set(SectionName.c_str(), "Trigger_L", GCMapping[i].AxisMapping.Tl);
		file.Set(SectionName.c_str(), "Trigger_R", GCMapping[i].AxisMapping.Tr);
		file.Set(SectionName.c_str(), "DeadZoneL", GCMapping[i].DeadZoneL);
		file.Set(SectionName.c_str(), "DeadZoneR", GCMapping[i].DeadZoneR);
		file.Set(SectionName.c_str(), "Diagonal", GCMapping[i].Diagonal);
		file.Set(SectionName.c_str(), "Square2Circle", GCMapping[i].bSquare2Circle);
		file.Set(SectionName.c_str(), "Rumble", GCMapping[i].Rumble);
		file.Set(SectionName.c_str(), "RumbleStrength", GCMapping[i].RumbleStrength);
		file.Set(SectionName.c_str(), "TriggerType", GCMapping[i].TriggerType);

		file.Set(SectionName.c_str(), "Source_Stick", GCMapping[i].Stick.Main);
		file.Set(SectionName.c_str(), "Source_CStick", GCMapping[i].Stick.Sub);
		file.Set(SectionName.c_str(), "Source_Shoulder", GCMapping[i].Stick.Shoulder);

		// ButtonMapping
		for (int x = 0; x < LAST_CONSTANT; x++)
			file.Set(SectionName.c_str(), gcControlNames[x], GCMapping[i].Button[x]);
	}

	file.Save(FULL_CONFIG_DIR "GCPad.ini");
}

// Load settings from file
// -----------------------
void Config::Load()
{
	// Load file
	IniFile file;
	file.Load(FULL_CONFIG_DIR "GCPad.ini");

	// ==================================================================
	// Global settings
	file.Get("General", "NoTriggerFilter", &g_Config.bNoTriggerFilter, false);

	for (int i = 0; i < 4; i++)
	{
		std::string SectionName = StringFromFormat("GCPad%i", i+1);

		file.Get(SectionName.c_str(), "DeviceID", &GCMapping[i].ID, 0);
		file.Get(SectionName.c_str(), "Axis_Lx", &GCMapping[i].AxisMapping.Lx, 0);
		file.Get(SectionName.c_str(), "Axis_Ly", &GCMapping[i].AxisMapping.Ly, 1);
		file.Get(SectionName.c_str(), "Axis_Rx", &GCMapping[i].AxisMapping.Rx, 2);
		file.Get(SectionName.c_str(), "Axis_Ry", &GCMapping[i].AxisMapping.Ry, 3);
		file.Get(SectionName.c_str(), "Trigger_L", &GCMapping[i].AxisMapping.Tl, 1004);
		file.Get(SectionName.c_str(), "Trigger_R", &GCMapping[i].AxisMapping.Tr, 1005);
		file.Get(SectionName.c_str(), "DeadZoneL", &GCMapping[i].DeadZoneL, 0);
		file.Get(SectionName.c_str(), "DeadZoneR", &GCMapping[i].DeadZoneR, 0);
		file.Get(SectionName.c_str(), "Diagonal", &GCMapping[i].Diagonal, 100);
		file.Get(SectionName.c_str(), "Square2Circle", &GCMapping[i].bSquare2Circle, false);
		file.Get(SectionName.c_str(), "Rumble", &GCMapping[i].Rumble, false);
		file.Get(SectionName.c_str(), "RumbleStrength", &GCMapping[i].RumbleStrength, 80);
		file.Get(SectionName.c_str(), "TriggerType", &GCMapping[i].TriggerType, 0);

		file.Get(SectionName.c_str(), "Source_Stick", &GCMapping[i].Stick.Main, 0);
		file.Get(SectionName.c_str(), "Source_CStick", &GCMapping[i].Stick.Sub, 0);
		file.Get(SectionName.c_str(), "Source_Shoulder", &GCMapping[i].Stick.Shoulder, 0);

		// ButtonMapping
		for (int x = 0; x < LAST_CONSTANT; x++)
			file.Get(SectionName.c_str(), gcControlNames[x], &GCMapping[i].Button[x], gcDefaultControls[x]);
	}
}

