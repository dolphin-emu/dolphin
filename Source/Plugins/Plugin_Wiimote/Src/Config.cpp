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
#include "StringUtil.h"
#include "wiimote_hid.h"
#include "Config.h"
#include "EmuDefinitions.h" // for PadMapping
#include "main.h"

// Configuration file control names
// Do not change the order unless you change the related arrays
// Directionals are ordered as L, R, U, D

static const char* wmControlNames[] =
{ 
	"WmA",
	"WmB",
	"Wm1",
	"Wm2",
	"WmP",
	"WmM",
	"WmH",
	"WmL",
	"WmR",
	"WmU",
	"WmD",
	"WmRollL",
	"WmRollR",
	"WmPitchU",
	"WmPitchD",
	"WmShake",

	"NcZ",
	"NcC",
	"NcL",
	"NcR",
	"NcU",
	"NcD",
	"NcRollL",
	"NcRollR",
	"NcPitchU",
	"NcPitchD",
	"NcShake",

	"CcA",
	"CcB",
	"CcX",
	"CcY",
	"CcP",
	"CcM",
	"CcH",
	"CcTl",
	"CcTr",
	"CcZl",
	"CcZr",
	"CcDl",
	"CcDr",
	"CcDu",
	"CcDd",
	"CcLl",
	"CcLr",
	"CcLu",
	"CcLd",
	"CcRl",
	"CcRr",
	"CcRu",
	"CcRd",

	"GH3Green",
	"GH3Red",
	"GH3Yellow",
	"GH3Blue",
	"GH3Orange",
	"GH3Plus",
	"GH3Minus",
	"GH3Whammy",
	"GH3Al",
	"GH3Ar",
	"GH3Au",
	"GH3Ad",
	"GH3StrumUp",
	"GH3StrumDown",
};

static int wmDefaultControls[] = 
{
// Wiimote
#ifdef _WIN32
	'Z',
	'X',
	'C',
	'V',
	VK_OEM_PLUS,
	VK_OEM_MINUS,
	VK_BACK,
	VK_LEFT,
	VK_RIGHT,
	VK_UP,
	VK_DOWN,
	'N',
	'M',
	VK_OEM_COMMA,
	VK_OEM_PERIOD,
	VK_OEM_2,	// /
#elif defined(HAVE_X11) && HAVE_X11
	XK_z,
	XK_x,
	XK_c,
	XK_v,
	XK_equal,
	XK_minus,
	XK_BackSpace,
	XK_Left,
	XK_Right,
	XK_Up,
	XK_Down,
	XK_n,
	XK_m,
	XK_comma,
	XK_period,
	XK_slash,
#else
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
#endif

// Nunchuck
#ifdef _WIN32
	VK_NUMPAD0,
	VK_DECIMAL,
	VK_NUMPAD4,
	VK_NUMPAD6,
	VK_NUMPAD8,
	VK_NUMPAD5,
	VK_NUMPAD7,
	VK_NUMPAD9,
	VK_NUMPAD1,
	VK_NUMPAD3,
	VK_NUMPAD2,
#elif defined(HAVE_X11) && HAVE_X11
	XK_KP_0,
	XK_KP_Decimal,
	XK_KP_4,
	XK_KP_6,
	XK_KP_8,
	XK_KP_5,
	XK_KP_7,
	XK_KP_9,
	XK_KP_1,
	XK_KP_3,
	XK_KP_2,
#else
	0,0,0,0,0,0,0,0,0,0,0,
#endif

// Classic Controller
// A, B, X, Y
#ifdef _WIN32
	VK_OEM_4, // [
	VK_OEM_6, // ]
	VK_OEM_1, // ;
	VK_OEM_7, // '
#elif defined(HAVE_X11) && HAVE_X11
	XK_bracketleft,
	XK_bracketright,
	XK_semicolon,
	XK_quoteright,
#else
	0,0,0,0,
#endif
// +, -, Home
	'H',
	'F',
	'G',
// Triggers, Zs
	'E',
	'Y',
	'R',
	'T',
// Digital pad
#ifdef _WIN32
	VK_NUMPAD4,
	VK_NUMPAD6,
	VK_NUMPAD8,
	VK_NUMPAD5,
#elif defined(HAVE_X11) && HAVE_X11
	XK_KP_4,
	XK_KP_6,
	XK_KP_8,
	XK_KP_5,
#else
	0,0,0,0,
#endif
// Left analog
	'A',
	'D',
	'W',
	'S',
// Right analog
	'J',
	'L',
	'I',
	'K',

// Guttar Hero
	'A',
	'S',
	'D',
	'F',
	'G',
	'L',
	'J',
	'H',
#ifdef _WIN32
	VK_NUMPAD4,
	VK_NUMPAD6,
	VK_NUMPAD8,
	VK_NUMPAD5,
#elif defined(HAVE_X11) && HAVE_X11
	XK_KP_4,
	XK_KP_6,
	XK_KP_8,
	XK_KP_5,
#else
	0,0,0,0,
#endif
	'I',
	'K',
};


Config g_Config;

Config::Config()
{
	// Set all default values to zero
	memset(this, 0, sizeof(Config));
}

void Config::Load()
{
	std::string temp;
	IniFile iniFile;
	iniFile.Load(FULL_CONFIG_DIR "Wiimote.ini");

	// Real Wiimote
	iniFile.Get("Real", "UpdateStatus", &bUpdateRealWiimote, true);
	iniFile.Get("Real", "AccNeutralX", &iAccNeutralX, 0);
	iniFile.Get("Real", "AccNeutralY", &iAccNeutralY, 0);
	iniFile.Get("Real", "AccNeutralZ", &iAccNeutralZ, 0);
	iniFile.Get("Real", "AccNunNeutralX", &iAccNunNeutralX, 0);
	iniFile.Get("Real", "AccNunNeutralY", &iAccNunNeutralY, 0);
	iniFile.Get("Real", "AccNunNeutralZ", &iAccNunNeutralZ, 0);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		// Slot specific settings
		char SectionName[32];
		sprintf(SectionName, "Wiimote%i", i + 1);

		// General
		iniFile.Get(SectionName, "Source", &WiiMoteEmu::WiiMapping[i].Source, (i == 0) ? 1 : 0);

		iniFile.Get(SectionName, "Sideways", &WiiMoteEmu::WiiMapping[i].bSideways, false);
		iniFile.Get(SectionName, "Upright", &WiiMoteEmu::WiiMapping[i].bUpright, false);
		iniFile.Get(SectionName, "ExtensionConnected", &WiiMoteEmu::WiiMapping[i].iExtensionConnected, WiiMoteEmu::EXT_NONE);
		iniFile.Get(SectionName, "MotionPlusConnected", &WiiMoteEmu::WiiMapping[i].bMotionPlusConnected, false);

		iniFile.Get(SectionName, "TiltInputWM", &WiiMoteEmu::WiiMapping[i].Tilt.InputWM, WiiMoteEmu::FROM_KEYBOARD);
		iniFile.Get(SectionName, "TiltInputNC", &WiiMoteEmu::WiiMapping[i].Tilt.InputNC, WiiMoteEmu::FROM_KEYBOARD);
		iniFile.Get(SectionName, "TiltRollDegree", &WiiMoteEmu::WiiMapping[i].Tilt.RollDegree, 60);
		iniFile.Get(SectionName, "TiltRollSwing", &WiiMoteEmu::WiiMapping[i].Tilt.RollSwing, false);
		iniFile.Get(SectionName, "TiltRollInvert", &WiiMoteEmu::WiiMapping[i].Tilt.RollInvert, false);
		WiiMoteEmu::WiiMapping[i].Tilt.RollRange = (WiiMoteEmu::WiiMapping[i].Tilt.RollSwing) ? 0 : WiiMoteEmu::WiiMapping[i].Tilt.RollDegree;
		iniFile.Get(SectionName, "TiltPitchDegree", &WiiMoteEmu::WiiMapping[i].Tilt.PitchDegree, 60);
		iniFile.Get(SectionName, "TiltPitchSwing", &WiiMoteEmu::WiiMapping[i].Tilt.PitchSwing, false);
		iniFile.Get(SectionName, "TiltPitchInvert", &WiiMoteEmu::WiiMapping[i].Tilt.PitchInvert, false);
		WiiMoteEmu::WiiMapping[i].Tilt.PitchRange = (WiiMoteEmu::WiiMapping[i].Tilt.PitchSwing) ? 0 : WiiMoteEmu::WiiMapping[i].Tilt.PitchDegree;

		// StickMapping
		iniFile.Get(SectionName, "NCStick", &WiiMoteEmu::WiiMapping[i].Stick.NC, WiiMoteEmu::FROM_KEYBOARD);
		iniFile.Get(SectionName, "CCStickLeft", &WiiMoteEmu::WiiMapping[i].Stick.CCL, WiiMoteEmu::FROM_KEYBOARD);
		iniFile.Get(SectionName, "CCStickRight", &WiiMoteEmu::WiiMapping[i].Stick.CCR, WiiMoteEmu::FROM_KEYBOARD);
		iniFile.Get(SectionName, "CCTriggers", &WiiMoteEmu::WiiMapping[i].Stick.CCT, WiiMoteEmu::FROM_KEYBOARD);
		iniFile.Get(SectionName, "GHStick", &WiiMoteEmu::WiiMapping[i].Stick.GH, WiiMoteEmu::FROM_KEYBOARD);

		// ButtonMapping
		for (int x = 0; x < WiiMoteEmu::LAST_CONSTANT; x++)
			iniFile.Get(SectionName, wmControlNames[x], &WiiMoteEmu::WiiMapping[i].Button[x], wmDefaultControls[x]);

		// This pad Id could possibly be higher than the number of pads that are connected,
		// but we check later, when needed, that that is not the case
		iniFile.Get(SectionName, "DeviceID", &WiiMoteEmu::WiiMapping[i].ID, 0);

		iniFile.Get(SectionName, "Axis_Lx", &WiiMoteEmu::WiiMapping[i].AxisMapping.Lx, 0);
		iniFile.Get(SectionName, "Axis_Ly", &WiiMoteEmu::WiiMapping[i].AxisMapping.Ly, 1);
		iniFile.Get(SectionName, "Axis_Rx", &WiiMoteEmu::WiiMapping[i].AxisMapping.Rx, 2);
		iniFile.Get(SectionName, "Axis_Ry", &WiiMoteEmu::WiiMapping[i].AxisMapping.Ry, 3);
		iniFile.Get(SectionName, "Trigger_L", &WiiMoteEmu::WiiMapping[i].AxisMapping.Tl, 1004);
		iniFile.Get(SectionName, "Trigger_R", &WiiMoteEmu::WiiMapping[i].AxisMapping.Tr, 1005);
		iniFile.Get(SectionName, "DeadZoneL", &WiiMoteEmu::WiiMapping[i].DeadZoneL, 0);
		iniFile.Get(SectionName, "DeadZoneR", &WiiMoteEmu::WiiMapping[i].DeadZoneR, 0);
		iniFile.Get(SectionName, "Diagonal", &WiiMoteEmu::WiiMapping[i].Diagonal, 100);
		iniFile.Get(SectionName, "Circle2Square", &WiiMoteEmu::WiiMapping[i].bCircle2Square, false);
		iniFile.Get(SectionName, "Rumble", &WiiMoteEmu::WiiMapping[i].Rumble, false);
		iniFile.Get(SectionName, "RumbleStrength", &WiiMoteEmu::WiiMapping[i].RumbleStrength, 80);
		iniFile.Get(SectionName, "TriggerType", &WiiMoteEmu::WiiMapping[i].TriggerType, 0);
	}

	Config::LoadIR();

	// Load a few screen settings to. If these are added to the DirectX plugin it's probably
	// better to place them in the main Dolphin.ini file
	iniFile.Load(FULL_CONFIG_DIR "gfx_opengl.ini");
	iniFile.Get("Settings", "KeepAR_4_3", &bKeepAR43, false);
	iniFile.Get("Settings", "KeepAR_16_9", &bKeepAR169, false);
	iniFile.Get("Settings", "Crop", &bCrop, false);

	//DEBUG_LOG(WIIMOTE, "Load()");
}

void Config::LoadIR()
{
	// Load the IR cursor settings if it's avaliable for the GameId, if not load the default settings
	IniFile iniFile;
	char TmpSection[32];
	int defaultLeft, defaultTop, defaultWidth, defaultHeight;

	sprintf(TmpSection, "%s", g_ISOId ? Hex2Ascii(g_ISOId).c_str() : "Default");
	iniFile.Load(FULL_CONFIG_DIR "IR Pointer.ini");
	//Load defaults first...
	iniFile.Get("Default", "IRLeft", &defaultLeft, LEFT);
	iniFile.Get("Default", "IRTop", &defaultTop, TOP);
	iniFile.Get("Default", "IRWidth", &defaultWidth, RIGHT - LEFT);
	iniFile.Get("Default", "IRHeight", &defaultHeight, BOTTOM - TOP);
	//...and fall back to them if the GameId is not found.
	//It shouldn't matter if we read Default twice, its in memory anyways.
	iniFile.Get(TmpSection, "IRLeft", &iIRLeft, defaultLeft);
	iniFile.Get(TmpSection, "IRTop", &iIRTop, defaultTop);
	iniFile.Get(TmpSection, "IRWidth", &iIRWidth, defaultWidth);
	iniFile.Get(TmpSection, "IRHeight", &iIRHeight, defaultHeight);
}

void Config::Save()
{
	IniFile iniFile;
	iniFile.Load(FULL_CONFIG_DIR "Wiimote.ini");
	
	iniFile.Set("Real", "UpdateStatus", bUpdateRealWiimote);
	iniFile.Set("Real", "AccNeutralX", iAccNeutralX);
	iniFile.Set("Real", "AccNeutralY", iAccNeutralY);
	iniFile.Set("Real", "AccNeutralZ", iAccNeutralZ);
	iniFile.Set("Real", "AccNunNeutralX", iAccNunNeutralX);
	iniFile.Set("Real", "AccNunNeutralY", iAccNunNeutralY);
	iniFile.Set("Real", "AccNunNeutralZ", iAccNunNeutralZ);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		// Slot specific settings
		char SectionName[32];
		sprintf(SectionName, "Wiimote%i", i + 1);

		iniFile.Set(SectionName, "Source", WiiMoteEmu::WiiMapping[i].Source);
		iniFile.Set(SectionName, "Sideways", WiiMoteEmu::WiiMapping[i].bSideways);
		iniFile.Set(SectionName, "Upright", WiiMoteEmu::WiiMapping[i].bUpright);
		iniFile.Set(SectionName, "ExtensionConnected", WiiMoteEmu::WiiMapping[i].iExtensionConnected);
		iniFile.Set(SectionName, "MotionPlusConnected", WiiMoteEmu::WiiMapping[i].bMotionPlusConnected);

		iniFile.Set(SectionName, "TiltInputWM", WiiMoteEmu::WiiMapping[i].Tilt.InputWM);
		iniFile.Set(SectionName, "TiltInputNC", WiiMoteEmu::WiiMapping[i].Tilt.InputNC);
		iniFile.Set(SectionName, "TiltRollDegree", WiiMoteEmu::WiiMapping[i].Tilt.RollDegree);
		iniFile.Set(SectionName, "TiltRollSwing", WiiMoteEmu::WiiMapping[i].Tilt.RollSwing);
		iniFile.Set(SectionName, "TiltRollInvert", WiiMoteEmu::WiiMapping[i].Tilt.RollInvert);
		iniFile.Set(SectionName, "TiltPitchDegree", WiiMoteEmu::WiiMapping[i].Tilt.PitchDegree);
		iniFile.Set(SectionName, "TiltPitchSwing", WiiMoteEmu::WiiMapping[i].Tilt.PitchSwing);
		iniFile.Set(SectionName, "TiltPitchInvert", WiiMoteEmu::WiiMapping[i].Tilt.PitchInvert);

		// StickMapping
		iniFile.Set(SectionName, "NCStick", WiiMoteEmu::WiiMapping[i].Stick.NC);
		iniFile.Set(SectionName, "CCStickLeft", WiiMoteEmu::WiiMapping[i].Stick.CCL);
		iniFile.Set(SectionName, "CCStickRight", WiiMoteEmu::WiiMapping[i].Stick.CCR);
		iniFile.Set(SectionName, "CCTriggers", WiiMoteEmu::WiiMapping[i].Stick.CCT);
		iniFile.Set(SectionName, "GHStick", WiiMoteEmu::WiiMapping[i].Stick.GH);

		// ButtonMapping
		for (int x = 0; x < WiiMoteEmu::LAST_CONSTANT; x++)
			iniFile.Set(SectionName, wmControlNames[x], WiiMoteEmu::WiiMapping[i].Button[x]);

		// Save the physical device ID number
		iniFile.Set(SectionName, "DeviceID", WiiMoteEmu::WiiMapping[i].ID);

		iniFile.Set(SectionName, "Axis_Lx", WiiMoteEmu::WiiMapping[i].AxisMapping.Lx);
		iniFile.Set(SectionName, "Axis_Ly", WiiMoteEmu::WiiMapping[i].AxisMapping.Ly);
		iniFile.Set(SectionName, "Axis_Rx", WiiMoteEmu::WiiMapping[i].AxisMapping.Rx);
		iniFile.Set(SectionName, "Axis_Ry", WiiMoteEmu::WiiMapping[i].AxisMapping.Ry);
		iniFile.Set(SectionName, "Trigger_L", WiiMoteEmu::WiiMapping[i].AxisMapping.Tl);
		iniFile.Set(SectionName, "Trigger_R", WiiMoteEmu::WiiMapping[i].AxisMapping.Tr);
		iniFile.Set(SectionName, "DeadZoneL", WiiMoteEmu::WiiMapping[i].DeadZoneL);
		iniFile.Set(SectionName, "DeadZoneR", WiiMoteEmu::WiiMapping[i].DeadZoneR);
		iniFile.Set(SectionName, "Diagonal", WiiMoteEmu::WiiMapping[i].Diagonal);
		iniFile.Set(SectionName, "Circle2Square", WiiMoteEmu::WiiMapping[i].bCircle2Square);
		iniFile.Set(SectionName, "Rumble", WiiMoteEmu::WiiMapping[i].Rumble);
		iniFile.Set(SectionName, "RumbleStrength", WiiMoteEmu::WiiMapping[i].RumbleStrength);
		iniFile.Set(SectionName, "TriggerType", WiiMoteEmu::WiiMapping[i].TriggerType);
	}

	iniFile.Save(FULL_CONFIG_DIR "Wiimote.ini");

	// Save the IR cursor settings if it's avaliable for the GameId, if not save the default settings
	iniFile.Load(FULL_CONFIG_DIR "IR Pointer.ini");
	char TmpSection[32];
	sprintf(TmpSection, "%s", g_ISOId ? Hex2Ascii(g_ISOId).c_str() : "Default");
	iniFile.Set(TmpSection, "IRLeft", iIRLeft);
	iniFile.Set(TmpSection, "IRTop", iIRTop);
	iniFile.Set(TmpSection, "IRWidth", iIRWidth);
	iniFile.Set(TmpSection, "IRHeight", iIRHeight);
	iniFile.Save(FULL_CONFIG_DIR "IR Pointer.ini");

	//DEBUG_LOG(WIIMOTE, "Save()");
}
