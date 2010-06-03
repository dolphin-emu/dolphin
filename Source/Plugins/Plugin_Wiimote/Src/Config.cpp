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
#include "FileUtil.h"

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
	iniFile.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "Wiimote.ini").c_str());

	// Real Wiimote
	Section& real = iniFile["Real"];

	real.Get("UpdateStatus", &bUpdateRealWiimote, true);
	real.Get("Unpair", &bUnpairRealWiimote, false);
	real.Get("Autopair", &bPairRealWiimote, false);
	real.Get("Timeout", &bWiiReadTimeout, 10);
	real.Get("AccNeutralX", &iAccNeutralX, 0);
	real.Get("AccNeutralY", &iAccNeutralY, 0);
	real.Get("AccNeutralZ", &iAccNeutralZ, 0);
	real.Get("AccNunNeutralX", &iAccNunNeutralX, 0);
	real.Get("AccNunNeutralY", &iAccNunNeutralY, 0);
	real.Get("AccNunNeutralZ", &iAccNunNeutralZ, 0);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		// Slot specific settings
		char SectionName[32];
		sprintf(SectionName, "Wiimote%i", i + 1);
		Section& section = iniFile[SectionName];

		// General
		section.Get("Source", &WiiMoteEmu::WiiMapping[i].Source, (i == 0) ? 1 : 0);

		section.Get("Sideways", &WiiMoteEmu::WiiMapping[i].bSideways, false);
		section.Get("Upright", &WiiMoteEmu::WiiMapping[i].bUpright, false);
		section.Get("ExtensionConnected", &WiiMoteEmu::WiiMapping[i].iExtensionConnected, WiiMoteEmu::EXT_NONE);
		section.Get("MotionPlusConnected", &WiiMoteEmu::WiiMapping[i].bMotionPlusConnected, false);

		section.Get("TiltInputWM", &WiiMoteEmu::WiiMapping[i].Tilt.InputWM, WiiMoteEmu::FROM_KEYBOARD);
		section.Get("TiltInputNC", &WiiMoteEmu::WiiMapping[i].Tilt.InputNC, WiiMoteEmu::FROM_KEYBOARD);
		section.Get("TiltRollDegree", &WiiMoteEmu::WiiMapping[i].Tilt.RollDegree, 60);
		section.Get("TiltRollSwing", &WiiMoteEmu::WiiMapping[i].Tilt.RollSwing, false);
		section.Get("TiltRollInvert", &WiiMoteEmu::WiiMapping[i].Tilt.RollInvert, false);
		WiiMoteEmu::WiiMapping[i].Tilt.RollRange = (WiiMoteEmu::WiiMapping[i].Tilt.RollSwing) ? 0 : WiiMoteEmu::WiiMapping[i].Tilt.RollDegree;
		section.Get("TiltPitchDegree", &WiiMoteEmu::WiiMapping[i].Tilt.PitchDegree, 60);
		section.Get("TiltPitchSwing", &WiiMoteEmu::WiiMapping[i].Tilt.PitchSwing, false);
		section.Get("TiltPitchInvert", &WiiMoteEmu::WiiMapping[i].Tilt.PitchInvert, false);
		WiiMoteEmu::WiiMapping[i].Tilt.PitchRange = (WiiMoteEmu::WiiMapping[i].Tilt.PitchSwing) ? 0 : WiiMoteEmu::WiiMapping[i].Tilt.PitchDegree;
		
		// StickMapping
		section.Get("NCStick", &WiiMoteEmu::WiiMapping[i].Stick.NC, WiiMoteEmu::FROM_KEYBOARD);
		section.Get("CCStickLeft", &WiiMoteEmu::WiiMapping[i].Stick.CCL, WiiMoteEmu::FROM_KEYBOARD);
		section.Get("CCStickRight", &WiiMoteEmu::WiiMapping[i].Stick.CCR, WiiMoteEmu::FROM_KEYBOARD);
		section.Get("CCTriggers", &WiiMoteEmu::WiiMapping[i].Stick.CCT, WiiMoteEmu::FROM_KEYBOARD);
		section.Get("GHStick", &WiiMoteEmu::WiiMapping[i].Stick.GH, WiiMoteEmu::FROM_KEYBOARD);

		// ButtonMapping
		for (int x = 0; x < WiiMoteEmu::LAST_CONSTANT; x++)
			section.Get(wmControlNames[x], &WiiMoteEmu::WiiMapping[i].Button[x], wmDefaultControls[x]);

		// This pad Id could possibly be higher than the number of pads that are connected,
		// but we check later, when needed, that that is not the case
		section.Get("DeviceID", &WiiMoteEmu::WiiMapping[i].ID, 0);

		section.Get("Axis_Lx", &WiiMoteEmu::WiiMapping[i].AxisMapping.Lx, 0);
		section.Get("Axis_Ly", &WiiMoteEmu::WiiMapping[i].AxisMapping.Ly, 1);
		section.Get("Axis_Rx", &WiiMoteEmu::WiiMapping[i].AxisMapping.Rx, 2);
		section.Get("Axis_Ry", &WiiMoteEmu::WiiMapping[i].AxisMapping.Ry, 3);
		section.Get("Trigger_L", &WiiMoteEmu::WiiMapping[i].AxisMapping.Tl, 1004);
		section.Get("Trigger_R", &WiiMoteEmu::WiiMapping[i].AxisMapping.Tr, 1005);
		section.Get("DeadZoneL", &WiiMoteEmu::WiiMapping[i].DeadZoneL, 0);
		section.Get("DeadZoneR", &WiiMoteEmu::WiiMapping[i].DeadZoneR, 0);
		section.Get("Diagonal", &WiiMoteEmu::WiiMapping[i].Diagonal, 100);
		section.Get("Circle2Square", &WiiMoteEmu::WiiMapping[i].bCircle2Square, false);
		section.Get("Rumble", &WiiMoteEmu::WiiMapping[i].Rumble, false);
		section.Get("RumbleStrength", &WiiMoteEmu::WiiMapping[i].RumbleStrength, 80);
		section.Get("TriggerType", &WiiMoteEmu::WiiMapping[i].TriggerType, 0);

		//UDPWii
		section.Get("UDPWii_Enable", &WiiMoteEmu::WiiMapping[i].UDPWM.enable, false);
		std::string port;
		char default_port[15];
		sprintf(default_port,"%d",4432+i);
		section.Get("UDPWii_Port", &port, default_port);
		strncpy(WiiMoteEmu::WiiMapping[i].UDPWM.port,port.c_str(),10);
		section.Get("UDPWii_EnableAccel", &WiiMoteEmu::WiiMapping[i].UDPWM.enableAccel, true);
		section.Get("UDPWii_EnableButtons", &WiiMoteEmu::WiiMapping[i].UDPWM.enableButtons, true);
		section.Get("UDPWii_EnableIR", &WiiMoteEmu::WiiMapping[i].UDPWM.enableIR, true);
		section.Get("UDPWii_EnableNunchuck", &WiiMoteEmu::WiiMapping[i].UDPWM.enableNunchuck, true);
	}

	iniFile.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "Dolphin.ini").c_str());
	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "Wiimote%i", i + 1);
		iniFile[SectionName].Get("AutoReconnectRealWiimote", &WiiMoteEmu::WiiMapping[i].bWiiAutoReconnect, false);
	}

	Config::LoadIR();

	// Load a few screen settings to. If these are added to the DirectX plugin it's probably
	// better to place them in the main Dolphin.ini file
	iniFile.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_opengl.ini").c_str());
	Section& settings = iniFile["Settings"];
	settings.Get("KeepAR_4_3", &bKeepAR43, false);
	settings.Get("KeepAR_16_9", &bKeepAR169, false);
	settings.Get("Crop", &bCrop, false);

	//DEBUG_LOG(WIIMOTE, "Load()");
}

void Config::LoadIR()
{
	// Load the IR cursor settings if it's avaliable for the GameId, if not load the default settings
	IniFile iniFile;
	char TmpSection[32];
	int defaultLeft, defaultTop, defaultWidth, defaultHeight;

	iniFile.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "IrPointer.ini").c_str());
	//Load defaults first...
	Section& def = iniFile["Default"];
	def.Get("IRLeft", &defaultLeft, LEFT);
	def.Get("IRTop", &defaultTop, TOP);
	def.Get("IRWidth", &defaultWidth, RIGHT - LEFT);
	def.Get("IRHeight", &defaultHeight, BOTTOM - TOP);
	def.Get("IRLevel", &iIRLevel, 3);

	//...and fall back to them if the GameId is not found.
	//It shouldn't matter if we read Default twice, its in memory anyways.
	sprintf(TmpSection, "%s", g_ISOId ? Hex2Ascii(g_ISOId).c_str() : "Default");
	Section& tmpsection = iniFile[TmpSection];
	tmpsection.Get("IRLeft", &iIRLeft, defaultLeft);
	tmpsection.Get("IRTop", &iIRTop, defaultTop);
	tmpsection.Get("IRWidth", &iIRWidth, defaultWidth);
	tmpsection.Get("IRHeight", &iIRHeight, defaultHeight);
}

void Config::Save()
{
	IniFile iniFile;
	iniFile.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "Wiimote.ini").c_str());
	
	Section& real = iniFile["Real"];
	real.Set("UpdateStatus", bUpdateRealWiimote);
	real.Set("Unpair", bUnpairRealWiimote);
	real.Set("Autopair", bPairRealWiimote);
	real.Set("Timeout", bWiiReadTimeout);
	real.Set("AccNeutralX", iAccNeutralX);
	real.Set("AccNeutralY", iAccNeutralY);
	real.Set("AccNeutralZ", iAccNeutralZ);
	real.Set("AccNunNeutralX", iAccNunNeutralX);
	real.Set("AccNunNeutralY", iAccNunNeutralY);
	real.Set("AccNunNeutralZ", iAccNunNeutralZ);

	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		// Slot specific settings
		char SectionName[32];
		sprintf(SectionName, "Wiimote%i", i + 1);
		Section& section = iniFile[SectionName];

		section.Set("Source", WiiMoteEmu::WiiMapping[i].Source);
		section.Set("Sideways", WiiMoteEmu::WiiMapping[i].bSideways);
		section.Set("Upright", WiiMoteEmu::WiiMapping[i].bUpright);
		section.Set("ExtensionConnected", WiiMoteEmu::WiiMapping[i].iExtensionConnected);
		section.Set("MotionPlusConnected", WiiMoteEmu::WiiMapping[i].bMotionPlusConnected);
		section.Set("TiltInputWM", WiiMoteEmu::WiiMapping[i].Tilt.InputWM);
		section.Set("TiltInputNC", WiiMoteEmu::WiiMapping[i].Tilt.InputNC);
		section.Set("TiltRollDegree", WiiMoteEmu::WiiMapping[i].Tilt.RollDegree);
		section.Set("TiltRollSwing", WiiMoteEmu::WiiMapping[i].Tilt.RollSwing);
		section.Set("TiltRollInvert", WiiMoteEmu::WiiMapping[i].Tilt.RollInvert);
		section.Set("TiltPitchDegree", WiiMoteEmu::WiiMapping[i].Tilt.PitchDegree);
		section.Set("TiltPitchSwing", WiiMoteEmu::WiiMapping[i].Tilt.PitchSwing);
		section.Set("TiltPitchInvert", WiiMoteEmu::WiiMapping[i].Tilt.PitchInvert);

		// StickMapping
		section.Set("NCStick", WiiMoteEmu::WiiMapping[i].Stick.NC);
		section.Set("CCStickLeft", WiiMoteEmu::WiiMapping[i].Stick.CCL);
		section.Set("CCStickRight", WiiMoteEmu::WiiMapping[i].Stick.CCR);
		section.Set("CCTriggers", WiiMoteEmu::WiiMapping[i].Stick.CCT);
		section.Set("GHStick", WiiMoteEmu::WiiMapping[i].Stick.GH);

		// ButtonMapping
		for (int x = 0; x < WiiMoteEmu::LAST_CONSTANT; x++)
			section.Set(wmControlNames[x], WiiMoteEmu::WiiMapping[i].Button[x]);

		// Save the physical device ID number
		section.Set("DeviceID", WiiMoteEmu::WiiMapping[i].ID);

		section.Set("Axis_Lx", WiiMoteEmu::WiiMapping[i].AxisMapping.Lx);
		section.Set("Axis_Ly", WiiMoteEmu::WiiMapping[i].AxisMapping.Ly);
		section.Set("Axis_Rx", WiiMoteEmu::WiiMapping[i].AxisMapping.Rx);
		section.Set("Axis_Ry", WiiMoteEmu::WiiMapping[i].AxisMapping.Ry);
		section.Set("Trigger_L", WiiMoteEmu::WiiMapping[i].AxisMapping.Tl);
		section.Set("Trigger_R", WiiMoteEmu::WiiMapping[i].AxisMapping.Tr);
		section.Set("DeadZoneL", WiiMoteEmu::WiiMapping[i].DeadZoneL);
		section.Set("DeadZoneR", WiiMoteEmu::WiiMapping[i].DeadZoneR);
		section.Set("Diagonal", WiiMoteEmu::WiiMapping[i].Diagonal);
		section.Set("Circle2Square", WiiMoteEmu::WiiMapping[i].bCircle2Square);
		section.Set("Rumble", WiiMoteEmu::WiiMapping[i].Rumble);
		section.Set("RumbleStrength", WiiMoteEmu::WiiMapping[i].RumbleStrength);
		section.Set("TriggerType", WiiMoteEmu::WiiMapping[i].TriggerType);

		// UDPWii
		section.Set("UDPWii_Enable", WiiMoteEmu::WiiMapping[i].UDPWM.enable);
		section.Set("UDPWii_Port", WiiMoteEmu::WiiMapping[i].UDPWM.port);
		section.Set("UDPWii_EnableAccel", WiiMoteEmu::WiiMapping[i].UDPWM.enableAccel);
		section.Set("UDPWii_EnableButtons", WiiMoteEmu::WiiMapping[i].UDPWM.enableButtons);
		section.Set("UDPWii_EnableIR", WiiMoteEmu::WiiMapping[i].UDPWM.enableIR);
		section.Set("UDPWii_EnableNunchuck", WiiMoteEmu::WiiMapping[i].UDPWM.enableNunchuck);
	}

	iniFile.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + "Wiimote.ini").c_str());

	// Save the IR cursor settings if it's avaliable for the GameId, if not save the default settings
	iniFile.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "IrPointer.ini").c_str());
	char TmpSection[32];
	sprintf(TmpSection, "%s", g_ISOId ? Hex2Ascii(g_ISOId).c_str() : "Default");
	Section& tmpsection = iniFile[TmpSection];
	tmpsection.Set("IRLeft", iIRLeft);
	tmpsection.Set("IRTop", iIRTop);
	tmpsection.Set("IRWidth", iIRWidth);
	tmpsection.Set("IRHeight", iIRHeight);
	tmpsection.Set("IRLevel", iIRLevel);
	iniFile.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + "IrPointer.ini").c_str());

	//Save any options that need to be accessed in Dolphin
	iniFile.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "Dolphin.ini").c_str());
	for (int i = 0; i < MAX_WIIMOTES; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "Wiimote%i", i + 1);
		iniFile[SectionName].Set("AutoReconnectRealWiimote", WiiMoteEmu::WiiMapping[i].bWiiAutoReconnect);
	}
	iniFile.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + "Dolphin.ini").c_str());

	//DEBUG_LOG(WIIMOTE, "Save()");
}
