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

#include "Config.h"
#include "EmuDefinitions.h" // for PadMapping
#include "main.h"

// Configuration file control names
// Do not change the order unless you change the related arrays
// Directionals are ordered as L, R, U, D

// Wiimote
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
	"WmShake",
	"WmPitchL",
	"WmPitchR",
};
static int wmDefaultControls[] = 
{
	'Z',
	'X',
	'C',
	'V',
	'M',
	'B',
	'N',
#ifdef _WIN32
	VK_LEFT,
	VK_RIGHT,
	VK_UP,
	VK_DOWN,
	VK_OEM_2, // /?
	VK_OEM_COMMA,
	VK_OEM_PERIOD
#elif defined(HAVE_X11) && HAVE_X11
	XK_Left,
	XK_Right,
	XK_Up,
	XK_Down,
	XK_slash,
	XK_comma,
	XK_period
#else
	0,0,0,0,0,0,0
#endif
};

// Nunchuk
static const char* ncControlNames[] =
{
	"NcZ",
	"NcC",
	"NcL",
	"NcR",
	"NcU",
	"NcD",
	"NcShake",
};
static int nCDefaultControls[] =
{
#ifdef _WIN32
	VK_NUMPAD0,
	VK_DECIMAL,
	VK_NUMPAD4,
	VK_NUMPAD6,
	VK_NUMPAD8,
	VK_NUMPAD5,
	VK_ADD
#elif defined(HAVE_X11) && HAVE_X11
	XK_KP_0,
	XK_KP_Decimal,
	XK_KP_4,
	XK_KP_6,
	XK_KP_8,
	XK_KP_5,
	XK_KP_Add
#else
	0,0,0,0,0,0,0
#endif
};

// Classic Controller
static const char* ccControlNames[] =
{
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
};
static int ccDefaultControls[] =
{
// A, B, X, Y
#ifdef _WIN32
	VK_OEM_6, // ]
	VK_OEM_7, // '
	VK_OEM_4, // [
	VK_OEM_1, // ;
#elif defined(HAVE_X11) && HAVE_X11
	XK_bracketright,
	XK_quoteright,
	XK_bracketleft,
	XK_semicolon,
#else
	0,0,0,0,
#endif
// +, -, Home
	'L',
	'J',
	'K',
// Triggers, Zs
	'E',
	'U',
	'R',
	'Y',
// Digital pad
	'A',
	'D',
	'W',
	'S',
// Left analog
	'F',
	'H',
	'T',
	'G',
// Right analog
#ifdef _WIN32
	VK_NUMPAD4,
	VK_NUMPAD6,
	VK_NUMPAD8,
	VK_NUMPAD5
#elif defined(HAVE_X11) && HAVE_X11
	XK_KP_4,
	XK_KP_6,
	XK_KP_8,
	XK_KP_5
#else
	0,0,0,0
#endif
};

// GH3 Default controls
static const char* gh3ControlNames[] =
{
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
static int GH3DefaultControls[] =
{
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

void Config::Load(bool ChangePad)
{
	std::string temp;
	IniFile iniFile;
	iniFile.Load(FULL_CONFIG_DIR "Wiimote.ini");

	// General
	iniFile.Get("Settings", "SidewaysDPad", &bSidewaysDPad, false);
	iniFile.Get("Settings", "ExtensionConnected", &iExtensionConnected, EXT_NONE);
	iniFile.Get("Settings", "MotionPlusConnected", &bMotionPlusConnected, false);

	// Real Wiimote
	iniFile.Get("Real", "Connect", &bConnectRealWiimote, true);
	iniFile.Get("Real", "Use", &bUseRealWiimote, true);
	iniFile.Get("Real", "UpdateStatus", &bUpdateRealWiimote, true);
	iniFile.Get("Real", "AccNeutralX", &iAccNeutralX, 0);
	iniFile.Get("Real", "AccNeutralY", &iAccNeutralY, 0);
	iniFile.Get("Real", "AccNeutralZ", &iAccNeutralZ, 0);
	iniFile.Get("Real", "AccNunNeutralX", &iAccNunNeutralX, 0);
	iniFile.Get("Real", "AccNunNeutralY", &iAccNunNeutralY, 0);
	iniFile.Get("Real", "AccNunNeutralZ", &iAccNunNeutralZ, 0);

	for (int i = 0; i < 1; i++)
	{
		// Slot specific settings
		char SectionName[32];
		sprintf(SectionName, "Wiimote%i", i + 1);
		iniFile.Get(SectionName, "NoTriggerFilter", &bNoTriggerFilter, false);
		iniFile.Get(SectionName, "TriggerType", &Trigger.Type, Trigger.TRIGGER_OFF);
		iniFile.Get(SectionName, "TriggerRollRange", &Trigger.Range.Roll, 50);
		iniFile.Get(SectionName, "TriggerPitchRange", &Trigger.Range.Pitch, false);

		// Wiimote
		for (int x = 0; x < WM_CONTROLS; x++)
			iniFile.Get(SectionName, wmControlNames[x], &WiiMoteEmu::PadMapping[i].Wm.keyForControls[x], wmDefaultControls[x]);
		// Nunchuck
		iniFile.Get(SectionName, "NunchuckStick", &Nunchuck.Type, Nunchuck.KEYBOARD);
		for (int x = 0; x < NC_CONTROLS; x++)
			iniFile.Get(SectionName, ncControlNames[x], &WiiMoteEmu::PadMapping[i].Nc.keyForControls[x], nCDefaultControls[x]);

		// Classic Controller
		iniFile.Get(SectionName, "CcLeftStick", &ClassicController.LType, ClassicController.KEYBOARD);
		iniFile.Get(SectionName, "CcRightStick", &ClassicController.RType, ClassicController.KEYBOARD);
		iniFile.Get(SectionName, "CcTriggers", &ClassicController.TType, ClassicController.KEYBOARD);
		for (int x = 0; x < CC_CONTROLS; x++)
			iniFile.Get(SectionName, ccControlNames[x], &WiiMoteEmu::PadMapping[i].Cc.keyForControls[x], ccDefaultControls[x]);
		iniFile.Get(SectionName, "GH3Analog", &GH3Controller.AType, GH3Controller.ANALOG1);
		for (int x = 0; x < GH3_CONTROLS; x++)
			iniFile.Get(SectionName, gh3ControlNames[x], &WiiMoteEmu::PadMapping[i].GH3c.keyForControls[x], GH3DefaultControls[x]);

		// Don't update this when we are loading settings from the ConfigBox
		if(!ChangePad)
		{
			// This pad Id could possibly be higher than the number of pads that are connected,
			// but we check later, when needed, that that is not the case
			iniFile.Get(SectionName, "DeviceID", &WiiMoteEmu::PadMapping[i].ID, 0);
			iniFile.Get(SectionName, "Enabled", &WiiMoteEmu::PadMapping[i].enabled, true);
		}

		// Joypad specific settings
			// Current joypad device ID: PadMapping[i].ID
			// Current joypad name: joyinfo[PadMapping[i].ID].Name

		// Prevent a crash from illegal access to joyinfo that will only have values for
		// the current amount of connected PadMapping
		if((u32)WiiMoteEmu::PadMapping[i].ID >= WiiMoteEmu::joyinfo.size()) continue;

		// Create a section name			
		std::string joySectionName = WiiMoteEmu::joyinfo[WiiMoteEmu::PadMapping[i].ID].Name;

		iniFile.Get(joySectionName.c_str(), "left_x", &WiiMoteEmu::PadMapping[i].Axis.Lx, 0);
		iniFile.Get(joySectionName.c_str(), "left_y", &WiiMoteEmu::PadMapping[i].Axis.Ly, 1);
		iniFile.Get(joySectionName.c_str(), "right_x", &WiiMoteEmu::PadMapping[i].Axis.Rx, 2);
		iniFile.Get(joySectionName.c_str(), "right_y", &WiiMoteEmu::PadMapping[i].Axis.Ry, 3);
		iniFile.Get(joySectionName.c_str(), "l_trigger", &WiiMoteEmu::PadMapping[i].Axis.Tl, 1004);
		iniFile.Get(joySectionName.c_str(), "r_trigger", &WiiMoteEmu::PadMapping[i].Axis.Tr, 1005);
		iniFile.Get(joySectionName.c_str(), "DeadZoneL", &WiiMoteEmu::PadMapping[i].DeadZoneL, 0);
		iniFile.Get(joySectionName.c_str(), "DeadZoneR", &WiiMoteEmu::PadMapping[i].DeadZoneR, 0);
		iniFile.Get(joySectionName.c_str(), "TriggerType", &WiiMoteEmu::PadMapping[i].triggertype, 0);
		iniFile.Get(joySectionName.c_str(), "Diagonal", &WiiMoteEmu::PadMapping[i].SDiagonal, "100%");
		iniFile.Get(joySectionName.c_str(), "Circle2Square", &WiiMoteEmu::PadMapping[i].bCircle2Square, false);
		iniFile.Get(joySectionName.c_str(), "RollInvert", &WiiMoteEmu::PadMapping[i].bRollInvert, false);
		iniFile.Get(joySectionName.c_str(), "PitchInvert", &WiiMoteEmu::PadMapping[i].bPitchInvert, false);
	}
	// Load the IR cursor settings if it's avaliable for the GameId, if not load the default settings
	iniFile.Load(FULL_CONFIG_DIR "IR Pointer.ini");
	char TmpSection[32];
	sprintf(TmpSection, "%s", g_ISOId ? Hex2Ascii(g_ISOId).c_str() : "Default");
	iniFile.Get(TmpSection, "IRLeft", &iIRLeft, LEFT);
	iniFile.Get(TmpSection, "IRTop", &iIRTop, TOP);
	iniFile.Get(TmpSection, "IRWidth", &iIRWidth, RIGHT - LEFT);
	iniFile.Get(TmpSection, "IRHeight", &iIRHeight, BOTTOM - TOP);

	// Load a few screen settings to. If these are added to the DirectX plugin it's probably
	// better to place them in the main Dolphin.ini file

	iniFile.Load(FULL_CONFIG_DIR "gfx_opengl.ini");
	iniFile.Get("Settings", "KeepAR_4_3", &bKeepAR43, false);
	iniFile.Get("Settings", "KeepAR_16_9", &bKeepAR169, false);
	iniFile.Get("Settings", "Crop", &bCrop, false);

	//DEBUG_LOG(WIIMOTE, "Load()");
}

void Config::Save(int Slot)
{
	IniFile iniFile;
	iniFile.Load(FULL_CONFIG_DIR "Wiimote.ini");
	iniFile.Set("Settings", "SidewaysDPad", bSidewaysDPad);
	iniFile.Set("Settings", "ExtensionConnected", iExtensionConnected);
	iniFile.Set("Settings", "MotionPlusConnected", bMotionPlusConnected);

	iniFile.Set("Real", "Connect", bConnectRealWiimote);	
	iniFile.Set("Real", "Use", bUseRealWiimote);
	iniFile.Set("Real", "UpdateStatus", bUpdateRealWiimote);
	iniFile.Set("Real", "AccNeutralX", iAccNeutralX);
	iniFile.Set("Real", "AccNeutralY", iAccNeutralY);
	iniFile.Set("Real", "AccNeutralZ", iAccNeutralZ);
	iniFile.Set("Real", "AccNunNeutralX", iAccNunNeutralX);
	iniFile.Set("Real", "AccNunNeutralY", iAccNunNeutralY);
	iniFile.Set("Real", "AccNunNeutralZ", iAccNunNeutralZ);

	for (int i = 0; i < 1; i++)
	{
		// Slot specific settings
		char SectionName[32];
		sprintf(SectionName, "Wiimote%i", i + 1);

		iniFile.Set(SectionName, "Enabled", WiiMoteEmu::PadMapping[i].enabled);
		iniFile.Set(SectionName, "NoTriggerFilter", bNoTriggerFilter);		
		iniFile.Set(SectionName, "TriggerType", Trigger.Type);
		iniFile.Set(SectionName, "TriggerRollRange", Trigger.Range.Roll);
		iniFile.Set(SectionName, "TriggerPitchRange", Trigger.Range.Pitch);

		// Wiimote
		for (int x = 0; x < WM_CONTROLS; x++)
			iniFile.Set(SectionName, wmControlNames[x], WiiMoteEmu::PadMapping[i].Wm.keyForControls[x]);

		// Nunchuck
		iniFile.Set(SectionName, "NunchuckStick", Nunchuck.Type);
		for (int x = 0; x < NC_CONTROLS; x++)
			iniFile.Set(SectionName, ncControlNames[x], WiiMoteEmu::PadMapping[i].Nc.keyForControls[x]);

		// Classic Controller
		iniFile.Set(SectionName, "CcLeftStick", ClassicController.LType);
		iniFile.Set(SectionName, "CcRightStick", ClassicController.RType);
		iniFile.Set(SectionName, "CcTriggers", ClassicController.TType);
		for (int x = 0; x < CC_CONTROLS; x++)
			iniFile.Set(SectionName, ccControlNames[x], WiiMoteEmu::PadMapping[i].Cc.keyForControls[x]);
		// GH3
		iniFile.Set(SectionName, "GH3Analog", GH3Controller.AType);
		for (int x = 0; x < GH3_CONTROLS; x++)
			iniFile.Set(SectionName, gh3ControlNames[x], WiiMoteEmu::PadMapping[i].GH3c.keyForControls[x]);

		// Save the physical device ID number
		iniFile.Set(SectionName, "DeviceID", WiiMoteEmu::PadMapping[i].ID);

		// Joypad specific settings
		// Current joypad device ID: PadMapping[i].ID
		// Current joypad name: joyinfo[PadMapping[i].ID].Name	

		// Save joypad specific settings. Check for "PadMapping[i].ID < SDL_NumJoysticks()" to
		// avoid reading a joyinfo that does't exist
		if((u32)WiiMoteEmu::PadMapping[i].ID >= WiiMoteEmu::joyinfo.size()) continue;

		// Create a new section name after the joypad name
		std::string joySectionName = WiiMoteEmu::joyinfo[WiiMoteEmu::PadMapping[i].ID].Name;

		iniFile.Set(joySectionName.c_str(), "left_x", WiiMoteEmu::PadMapping[i].Axis.Lx);
		iniFile.Set(joySectionName.c_str(), "left_y", WiiMoteEmu::PadMapping[i].Axis.Ly);
		iniFile.Set(joySectionName.c_str(), "right_x", WiiMoteEmu::PadMapping[i].Axis.Rx);
		iniFile.Set(joySectionName.c_str(), "right_y", WiiMoteEmu::PadMapping[i].Axis.Ry);
		iniFile.Set(joySectionName.c_str(), "l_trigger", WiiMoteEmu::PadMapping[i].Axis.Tl);
		iniFile.Set(joySectionName.c_str(), "r_trigger", WiiMoteEmu::PadMapping[i].Axis.Tr);

		iniFile.Set(joySectionName.c_str(), "DeadZoneL", WiiMoteEmu::PadMapping[i].DeadZoneL);
		iniFile.Set(joySectionName.c_str(), "DeadZoneR", WiiMoteEmu::PadMapping[i].DeadZoneR);
		//iniFile.Set(joySectionName.c_str(), "controllertype", WiiMoteEmu::PadMapping[i].controllertype);
		iniFile.Set(joySectionName.c_str(), "TriggerType", WiiMoteEmu::PadMapping[i].triggertype);
		iniFile.Set(joySectionName.c_str(), "Diagonal", WiiMoteEmu::PadMapping[i].SDiagonal);
		iniFile.Set(joySectionName.c_str(), "Circle2Square", WiiMoteEmu::PadMapping[i].bCircle2Square);
		iniFile.Set(joySectionName.c_str(), "RollInvert", WiiMoteEmu::PadMapping[i].bRollInvert);
		iniFile.Set(joySectionName.c_str(), "PitchInvert", WiiMoteEmu::PadMapping[i].bPitchInvert);
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
