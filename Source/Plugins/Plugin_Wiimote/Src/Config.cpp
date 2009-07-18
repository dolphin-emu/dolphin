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


#include <iostream>

#include "Common.h"
#include "IniFile.h"
#include "StringUtil.h"

#include "Config.h"
#include "EmuDefinitions.h" // for PadMapping
#include "main.h"

// Ini Control Names
// Do not change the order unless you change the related arrays
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
	"CcZl",
	"CcZr",
	"CcTr",
	"CcDl",
	"CcDu",
	"CcDr",
	"CcDd",
	"CcLl",
	"CcLu",
	"CcLr",
	"CcLd",
	"CcRl",
	"CcRu",
	"CcRr",
	"CcRd",
};

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
	"GH3Au",
	"GH3Ar",
	"GH3Ad",
	"GH3StrumUp",
	"GH3StrumDown",
};
// Default controls
static int wmDefaultControls[] = 
{
	65, // WmA
	66, // WmB
	49, // Wm1
	50, // Wm2
	80, // WmP
	77, // WmM
	72, // WmH
#ifdef _WIN32
	VK_LEFT, // Regular directional keys
	VK_RIGHT,
	VK_UP,
	VK_DOWN,
#elif defined(HAVE_X11) && HAVE_X11
	XK_Left,
	XK_Right,
	XK_Up,
	XK_Down,
#else
	0,0,0,0,
#endif
	83, // WmShake (S)
	51, // WmPitchL (3)
	52, // WmPitchR (4)
};

static int nCDefaultControls[] =
{
	90, // NcZ Z
	67, // NcC C
#ifdef _WIN32
	VK_NUMPAD4, // NcL
	VK_NUMPAD6, // NcR
	VK_NUMPAD8, // NcU
	VK_NUMPAD5, // NcD
#elif defined(HAVE_X11) && HAVE_X11
	XK_KP_Left, // Numlock must be off
	XK_KP_Right,
	XK_KP_Up,
	XK_KP_Down,
#else
	0,0,0,0,
#endif
	68, // NcShake D
};

static int ccDefaultControls[] =
{
	90, // CcA (C)
	67, // CcB (Z)
	0x58, // CcX (X)
	0x59, // CcY (Y)
	0x4f, // CcP O instead of P
	0x4e, // CcM N instead of M
	0x55, // CcH U instead of H
	0x37, // CcTl 7
	0x38, // CcZl 8
	0x39, // CcZr 9
	0x30, // CcTr 0
#ifdef _WIN32		//  TODO: ugly that wm/nc use lrud and cc/gh3 use lurd
	VK_NUMPAD4, //CcDl
	VK_NUMPAD8, // CcDu
	VK_NUMPAD6, // CcDr
	VK_NUMPAD5, // CcDd
#elif defined(HAVE_X11) && HAVE_X11
	XK_KP_Left, // Numlock must be off
	XK_KP_Up,
	XK_KP_Right,
	XK_KP_Down,
#else
	0,0,0,0,
#endif
	0x4a, // CcLl J
	0x49, // CcLu I
	0x4c, // CcLr L
	0x4b, // CcLd K
	0x44, // CcRl D 
	0x52, // CcRu R
	0x47, // CcRr G
	0x46, // CcRd F
};

static int GH3DefaultControls[] =
{
	0, // GH3Green
	0, // GH3Red
	0, // GH3Yellow
	0, // GH3Blue
	0, // GH3Orange
	0, // GH3Plus
	0, // GH3Minus
	0, // GH3Whammy
	0, // GH3Al
	0, // GH3Au
	0, // GH3Ar
	0, // GH3Ad
	13, // GH3StrumUp
	161, // GH3StrumDown
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
		for (int x = 0; x < 14; x++)
			iniFile.Get(SectionName, wmControlNames[x], &WiiMoteEmu::PadMapping[i].Wm.keyForControls[x], wmDefaultControls[x]);
		// Nunchuck
		iniFile.Get(SectionName, "NunchuckStick", &Nunchuck.Type, Nunchuck.KEYBOARD);
		for (int x = 0; x < 7; x++)
			iniFile.Get(SectionName, ncControlNames[x], &WiiMoteEmu::PadMapping[i].Nc.keyForControls[x], nCDefaultControls[x]);

		// Classic Controller
		iniFile.Get(SectionName, "CcLeftStick", &ClassicController.LType, ClassicController.KEYBOARD);
		iniFile.Get(SectionName, "CcRightStick", &ClassicController.RType, ClassicController.KEYBOARD);
		iniFile.Get(SectionName, "CcTriggers", &ClassicController.TType, ClassicController.KEYBOARD);
		for (int x = 0; x < 23; x++)
			iniFile.Get(SectionName, ccControlNames[x], &WiiMoteEmu::PadMapping[i].Cc.keyForControls[x], ccDefaultControls[x]);
		iniFile.Get(SectionName, "GH3Analog", &GH3Controller.AType, GH3Controller.ANALOG1);
		for (int x = 0; x < 14; x++)
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

	//INFO_LOG(CONSOLE, "Load()\n");
}

void Config::Save(int Slot)
{
	IniFile iniFile;
	iniFile.Load(FULL_CONFIG_DIR "Wiimote.ini");
	iniFile.Set("Settings", "SidewaysDPad", bSidewaysDPad);
	iniFile.Set("Settings", "ExtensionConnected", iExtensionConnected);

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
		for (int x = 0; x < 14; x++)
			iniFile.Set(SectionName, wmControlNames[x], WiiMoteEmu::PadMapping[i].Wm.keyForControls[x]);

		// Nunchuck
		iniFile.Set(SectionName, "NunchuckStick", Nunchuck.Type);
		for (int x = 0; x < 7; x++)
			iniFile.Set(SectionName, ncControlNames[x], WiiMoteEmu::PadMapping[i].Nc.keyForControls[x]);

		// Classic Controller
		iniFile.Set(SectionName, "CcLeftStick", ClassicController.LType);
		iniFile.Set(SectionName, "CcRightStick", ClassicController.RType);
		iniFile.Set(SectionName, "CcTriggers", ClassicController.TType);
		for (int x = 0; x < 23; x++)
			iniFile.Set(SectionName, ccControlNames[x], WiiMoteEmu::PadMapping[i].Cc.keyForControls[x]);
		// GH3
		iniFile.Set(SectionName, "GH3Analog", GH3Controller.AType);
		for (int x = 0; x < 14; x++)
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

	//INFO_LOG(CONSOLE, "Save()\n");
}