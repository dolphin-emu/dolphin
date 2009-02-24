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


#include <iostream>

#include "Common.h"
#include "IniFile.h"
#include "StringUtil.h"

#include "Config.h"
#include "EmuDefinitions.h" // for PadMapping
#include "main.h"

// TODO: Figure out what to do for non-Win32
#ifndef _WIN32
#define VK_LEFT 0
#define VK_RIGHT 0
#define VK_UP 0
#define VK_DOWN 0
#define VK_NUMPAD4 0
#define VK_NUMPAD6 0
#define VK_NUMPAD8 0
#define VK_NUMPAD5 0
#endif

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
	iniFile.Get("Settings", "NunchuckConnected", &bNunchuckConnected, false);
	iniFile.Get("Settings", "ClassicControllerConnected", &bClassicControllerConnected, false);

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

	// Load the IR cursor settings if it's avaliable, if not load the default settings
	std::string TmpSection;
	if (g_ISOId) TmpSection = Hex2Ascii(g_ISOId); else TmpSection = "Emulated";
	iniFile.Get(TmpSection.c_str(), "IRLeft", &iIRLeft, LEFT);
	iniFile.Get(TmpSection.c_str(), "IRTop", &iIRTop, TOP);
	iniFile.Get(TmpSection.c_str(), "IRWidth", &iIRWidth, RIGHT - LEFT);
	iniFile.Get(TmpSection.c_str(), "IRHeight", &iIRHeight, BOTTOM - TOP);

	// Default controls
		int WmA = 65, WmB = 66,
			Wm1 = 49, Wm2 = 50,
			WmP = 80, WmM = 77, WmH = 72,
			WmL = VK_LEFT, WmR = VK_RIGHT, WmU = VK_UP, WmD = VK_DOWN, // Regular directional keys
			WmShake = 83, // S
			WmPitchL = 51, WmPitchR = 52, // 3 and 4
			
			NcZ = 90, NcC = 67, // C, Z
			NcL = VK_NUMPAD4, NcR = VK_NUMPAD6, NcU = VK_NUMPAD8, NcD = VK_NUMPAD5, // Numpad
			NcShake = 68, // D

			CcA = 90, CcB = 67, CcX = 0x58, CcY = 0x59, // C, Z, X, Y
			CcP = 0x4f, CcM = 0x4e, CcH = 0x55, // O instead of P, N instead of M, U instead of H
			CcTl = 0x37, CcZl = 0x38, CcZr = 0x39, CcTr = 0x30, // 7, 8, 9, 0
			CcDl = VK_NUMPAD4, CcDu = VK_NUMPAD8, CcDr = VK_NUMPAD6, CcDd = VK_NUMPAD5, // Numpad
			CcLl = 0x4a, CcLu = 0x49, CcLr = 0x4c, CcLd = 0x4b, // J, I, L, K
			CcRl = 0x44, CcRu = 0x52, CcRr = 0x47, CcRd = 0x46; // D, R, G, F

	for (int i = 0; i < 1; i++)
	{
		// ==================================================================
		// Slot specific settings
		// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
		std::string SectionName = StringFromFormat("Wiimote%i", i + 1);
		iniFile.Get(SectionName.c_str(), "NoTriggerFilter", &bNoTriggerFilter, false);
		iniFile.Get(SectionName.c_str(), "TriggerType", &Trigger.Type, Trigger.TRIGGER_OFF);
		iniFile.Get(SectionName.c_str(), "TriggerRollRange", &Trigger.Range.Roll, 50);
		iniFile.Get(SectionName.c_str(), "TriggerPitchRange", &Trigger.Range.Pitch, false);

		// Wiimote
		iniFile.Get(SectionName.c_str(), "WmA", &WiiMoteEmu::PadMapping[i].Wm.A, WmA);
		iniFile.Get(SectionName.c_str(), "WmB", &WiiMoteEmu::PadMapping[i].Wm.B, WmB);
		iniFile.Get(SectionName.c_str(), "Wm1", &WiiMoteEmu::PadMapping[i].Wm.One, Wm1);
		iniFile.Get(SectionName.c_str(), "Wm2", &WiiMoteEmu::PadMapping[i].Wm.Two, Wm2);
		iniFile.Get(SectionName.c_str(), "WmP", &WiiMoteEmu::PadMapping[i].Wm.P, WmP);
		iniFile.Get(SectionName.c_str(), "WmM", &WiiMoteEmu::PadMapping[i].Wm.M, WmM);
		iniFile.Get(SectionName.c_str(), "WmH", &WiiMoteEmu::PadMapping[i].Wm.H, WmH);
		iniFile.Get(SectionName.c_str(), "WmL", &WiiMoteEmu::PadMapping[i].Wm.L, WmL);
		iniFile.Get(SectionName.c_str(), "WmR", &WiiMoteEmu::PadMapping[i].Wm.R, WmR);
		iniFile.Get(SectionName.c_str(), "WmU", &WiiMoteEmu::PadMapping[i].Wm.U, WmU);
		iniFile.Get(SectionName.c_str(), "WmD", &WiiMoteEmu::PadMapping[i].Wm.D, WmD);
		iniFile.Get(SectionName.c_str(), "WmShake", &WiiMoteEmu::PadMapping[i].Wm.Shake, WmShake);
		iniFile.Get(SectionName.c_str(), "WmPitchL", &WiiMoteEmu::PadMapping[i].Wm.PitchL, WmPitchL);
		iniFile.Get(SectionName.c_str(), "WmPitchR", &WiiMoteEmu::PadMapping[i].Wm.PitchR, WmPitchR);

		// Nunchuck
		iniFile.Get(SectionName.c_str(), "NunchuckStick", &Nunchuck.Type, Nunchuck.KEYBOARD);
		iniFile.Get(SectionName.c_str(), "NcZ", &WiiMoteEmu::PadMapping[i].Nc.Z, NcZ);
		iniFile.Get(SectionName.c_str(), "NcC", &WiiMoteEmu::PadMapping[i].Nc.C, NcC);
		iniFile.Get(SectionName.c_str(), "NcL", &WiiMoteEmu::PadMapping[i].Nc.L, NcL);
		iniFile.Get(SectionName.c_str(), "NcR", &WiiMoteEmu::PadMapping[i].Nc.R, NcR);	
		iniFile.Get(SectionName.c_str(), "NcU", &WiiMoteEmu::PadMapping[i].Nc.U, NcU);
		iniFile.Get(SectionName.c_str(), "NcD", &WiiMoteEmu::PadMapping[i].Nc.D, NcD);
		iniFile.Get(SectionName.c_str(), "NcShake", &WiiMoteEmu::PadMapping[i].Nc.Shake, NcShake);

		// Classic Controller
		iniFile.Get(SectionName.c_str(), "CcLeftStick", &ClassicController.LType, ClassicController.KEYBOARD);
		iniFile.Get(SectionName.c_str(), "CcRightStick", &ClassicController.RType, ClassicController.KEYBOARD);
		iniFile.Get(SectionName.c_str(), "CcTriggers", &ClassicController.TType, ClassicController.KEYBOARD);
		iniFile.Get(SectionName.c_str(), "CcA", &WiiMoteEmu::PadMapping[i].Cc.A, CcA);
		iniFile.Get(SectionName.c_str(), "CcB", &WiiMoteEmu::PadMapping[i].Cc.B, CcB);
		iniFile.Get(SectionName.c_str(), "CcX", &WiiMoteEmu::PadMapping[i].Cc.X, CcX);
		iniFile.Get(SectionName.c_str(), "CcY", &WiiMoteEmu::PadMapping[i].Cc.Y, CcY);	
		iniFile.Get(SectionName.c_str(), "CcP", &WiiMoteEmu::PadMapping[i].Cc.P, CcP);
		iniFile.Get(SectionName.c_str(), "CcM", &WiiMoteEmu::PadMapping[i].Cc.M, CcM);
		iniFile.Get(SectionName.c_str(), "CcH", &WiiMoteEmu::PadMapping[i].Cc.H, CcH);
		iniFile.Get(SectionName.c_str(), "CcTl", &WiiMoteEmu::PadMapping[i].Cc.Tl, CcTl);
		iniFile.Get(SectionName.c_str(), "CcZl", &WiiMoteEmu::PadMapping[i].Cc.Zl, CcZl);
		iniFile.Get(SectionName.c_str(), "CcZr", &WiiMoteEmu::PadMapping[i].Cc.Zr, CcZr);
		iniFile.Get(SectionName.c_str(), "CcTr", &WiiMoteEmu::PadMapping[i].Cc.Tr, CcTr);
		iniFile.Get(SectionName.c_str(), "CcDl", &WiiMoteEmu::PadMapping[i].Cc.Dl, CcDl);
		iniFile.Get(SectionName.c_str(), "CcDu", &WiiMoteEmu::PadMapping[i].Cc.Du, CcDu);
		iniFile.Get(SectionName.c_str(), "CcDr", &WiiMoteEmu::PadMapping[i].Cc.Dr, CcDr);
		iniFile.Get(SectionName.c_str(), "CcDd", &WiiMoteEmu::PadMapping[i].Cc.Dd, CcDd);
		iniFile.Get(SectionName.c_str(), "CcLl", &WiiMoteEmu::PadMapping[i].Cc.Ll, CcLl);
		iniFile.Get(SectionName.c_str(), "CcLu", &WiiMoteEmu::PadMapping[i].Cc.Lu, CcLu);
		iniFile.Get(SectionName.c_str(), "CcLr", &WiiMoteEmu::PadMapping[i].Cc.Lr, CcLr);
		iniFile.Get(SectionName.c_str(), "CcLd", &WiiMoteEmu::PadMapping[i].Cc.Ld, CcLd);
		iniFile.Get(SectionName.c_str(), "CcRl", &WiiMoteEmu::PadMapping[i].Cc.Rl, CcRl);
		iniFile.Get(SectionName.c_str(), "CcRu", &WiiMoteEmu::PadMapping[i].Cc.Ru, CcRu);
		iniFile.Get(SectionName.c_str(), "CcRr", &WiiMoteEmu::PadMapping[i].Cc.Rr, CcRr);
		iniFile.Get(SectionName.c_str(), "CcRd", &WiiMoteEmu::PadMapping[i].Cc.Rd, CcRd);

		// Don't update this when we are loading settings from the ConfigBox
		if(!ChangePad)
		{
			/* This pad Id could possibly be higher than the number of pads that are connected,
			   but we check later, when needed, that that is not the case */
			iniFile.Get(SectionName.c_str(), "DeviceID", &WiiMoteEmu::PadMapping[i].ID, 0);
			iniFile.Get(SectionName.c_str(), "Enabled", &WiiMoteEmu::PadMapping[i].enabled, true);
		}
		// ===================

		// ==================================================================
		// Joypad specific settings
		// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
			// Current joypad device ID: PadMapping[i].ID
			// Current joypad name: joyinfo[PadMapping[i].ID].Name

		/* Prevent a crash from illegal access to joyinfo that will only have values for
		   the current amount of connected PadMapping */
		if(WiiMoteEmu::PadMapping[i].ID >= WiiMoteEmu::joyinfo.size()) continue;

		// Create a section name			
		SectionName = WiiMoteEmu::joyinfo[WiiMoteEmu::PadMapping[i].ID].Name;

		iniFile.Get(SectionName.c_str(), "left_x", &WiiMoteEmu::PadMapping[i].Axis.Lx, 0);	
		iniFile.Get(SectionName.c_str(), "left_y", &WiiMoteEmu::PadMapping[i].Axis.Ly, 1);	
		iniFile.Get(SectionName.c_str(), "right_x", &WiiMoteEmu::PadMapping[i].Axis.Rx, 2);	
		iniFile.Get(SectionName.c_str(), "right_y", &WiiMoteEmu::PadMapping[i].Axis.Ry, 3);
		iniFile.Get(SectionName.c_str(), "l_trigger", &WiiMoteEmu::PadMapping[i].Axis.Tl, 1004);
		iniFile.Get(SectionName.c_str(), "r_trigger", &WiiMoteEmu::PadMapping[i].Axis.Tr, 1005);
		iniFile.Get(SectionName.c_str(), "DeadZoneL", &WiiMoteEmu::PadMapping[i].DeadZoneL, 0);
		iniFile.Get(SectionName.c_str(), "DeadZoneR", &WiiMoteEmu::PadMapping[i].DeadZoneR, 0);
		iniFile.Get(SectionName.c_str(), "TriggerType", &WiiMoteEmu::PadMapping[i].triggertype, 0);
		iniFile.Get(SectionName.c_str(), "Diagonal", &WiiMoteEmu::PadMapping[i].SDiagonal, "100%");
		iniFile.Get(SectionName.c_str(), "Circle2Square", &WiiMoteEmu::PadMapping[i].bCircle2Square, false);	
		iniFile.Get(SectionName.c_str(), "RollInvert", &WiiMoteEmu::PadMapping[i].bRollInvert, false);
		iniFile.Get(SectionName.c_str(), "PitchInvert", &WiiMoteEmu::PadMapping[i].bPitchInvert, false);
	}
	// =============================
	Console::Print("Load()\n");
}

void Config::Save(int Slot)
{
    IniFile iniFile;
    iniFile.Load(FULL_CONFIG_DIR "Wiimote.ini");
    iniFile.Set("Settings", "SidewaysDPad", bSidewaysDPad);
	iniFile.Set("Settings", "NunchuckConnected", bNunchuckConnected);
	iniFile.Set("Settings", "ClassicControllerConnected", bClassicControllerConnected);

	iniFile.Set("Real", "Connect", bConnectRealWiimote);	
	iniFile.Set("Real", "Use", bUseRealWiimote);
	iniFile.Set("Real", "UpdateStatus", bUpdateRealWiimote);
	iniFile.Set("Real", "AccNeutralX", iAccNeutralX);
	iniFile.Set("Real", "AccNeutralY", iAccNeutralY);
	iniFile.Set("Real", "AccNeutralZ", iAccNeutralZ);
	iniFile.Set("Real", "AccNunNeutralX", iAccNunNeutralX);
	iniFile.Set("Real", "AccNunNeutralY", iAccNunNeutralY);
	iniFile.Set("Real", "AccNunNeutralZ", iAccNunNeutralZ);

	// Save the IR cursor settings if it's avaliable, if not save the default settings
	std::string TmpSection;
	if (g_ISOId) TmpSection = Hex2Ascii(g_ISOId); else TmpSection = "Emulated";
	iniFile.Set(TmpSection.c_str(), "IRLeft", iIRLeft);
	iniFile.Set(TmpSection.c_str(), "IRTop", iIRTop);
	iniFile.Set(TmpSection.c_str(), "IRWidth", iIRWidth);
	iniFile.Set(TmpSection.c_str(), "IRHeight", iIRHeight);

	for (int i = 0; i < 1; i++)
	{
		// ==================================================================
		// Slot specific settings
		// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
		std::string SectionName = StringFromFormat("Wiimote%i", i + 1);
		iniFile.Set(SectionName.c_str(), "Enabled", WiiMoteEmu::PadMapping[i].enabled);
		iniFile.Set(SectionName.c_str(), "NoTriggerFilter", bNoTriggerFilter);		
		iniFile.Set(SectionName.c_str(), "TriggerType", Trigger.Type);
		iniFile.Set(SectionName.c_str(), "TriggerRollRange", Trigger.Range.Roll);
		iniFile.Set(SectionName.c_str(), "TriggerPitchRange", Trigger.Range.Pitch);

		// Wiimote
		iniFile.Set(SectionName.c_str(), "WmA", WiiMoteEmu::PadMapping[i].Wm.A);
		iniFile.Set(SectionName.c_str(), "WmB", WiiMoteEmu::PadMapping[i].Wm.B);
		iniFile.Set(SectionName.c_str(), "Wm1", WiiMoteEmu::PadMapping[i].Wm.One);
		iniFile.Set(SectionName.c_str(), "Wm2", WiiMoteEmu::PadMapping[i].Wm.Two);
		iniFile.Set(SectionName.c_str(), "WmP", WiiMoteEmu::PadMapping[i].Wm.P);
		iniFile.Set(SectionName.c_str(), "WmM", WiiMoteEmu::PadMapping[i].Wm.M);
		iniFile.Set(SectionName.c_str(), "WmH", WiiMoteEmu::PadMapping[i].Wm.H);
		iniFile.Set(SectionName.c_str(), "WmL", WiiMoteEmu::PadMapping[i].Wm.L);
		iniFile.Set(SectionName.c_str(), "WmR", WiiMoteEmu::PadMapping[i].Wm.R);
		iniFile.Set(SectionName.c_str(), "WmU", WiiMoteEmu::PadMapping[i].Wm.U);
		iniFile.Set(SectionName.c_str(), "WmD", WiiMoteEmu::PadMapping[i].Wm.D);
		iniFile.Set(SectionName.c_str(), "WmShake", WiiMoteEmu::PadMapping[i].Wm.Shake);
		iniFile.Set(SectionName.c_str(), "WmPitchL", WiiMoteEmu::PadMapping[i].Wm.PitchL);
		iniFile.Set(SectionName.c_str(), "WmPitchR", WiiMoteEmu::PadMapping[i].Wm.PitchR);

		// Nunchuck
		iniFile.Set(SectionName.c_str(), "NunchuckStick", Nunchuck.Type);
		iniFile.Set(SectionName.c_str(), "NcZ", WiiMoteEmu::PadMapping[i].Nc.Z);
		iniFile.Set(SectionName.c_str(), "NcC", WiiMoteEmu::PadMapping[i].Nc.C);
		iniFile.Set(SectionName.c_str(), "NcL", WiiMoteEmu::PadMapping[i].Nc.L);
		iniFile.Set(SectionName.c_str(), "NcR", WiiMoteEmu::PadMapping[i].Nc.R);
		iniFile.Set(SectionName.c_str(), "NcU", WiiMoteEmu::PadMapping[i].Nc.U);
		iniFile.Set(SectionName.c_str(), "NcD", WiiMoteEmu::PadMapping[i].Nc.D);
		iniFile.Set(SectionName.c_str(), "NcShake", WiiMoteEmu::PadMapping[i].Nc.Shake);

		// Classic Controller
		iniFile.Set(SectionName.c_str(), "CcLeftStick", ClassicController.LType);
		iniFile.Set(SectionName.c_str(), "CcRightStick", ClassicController.RType);
		iniFile.Set(SectionName.c_str(), "CcTriggers", ClassicController.TType);
		iniFile.Set(SectionName.c_str(), "CcA", WiiMoteEmu::PadMapping[i].Cc.A);
		iniFile.Set(SectionName.c_str(), "CcB", WiiMoteEmu::PadMapping[i].Cc.B);
		iniFile.Set(SectionName.c_str(), "CcX", WiiMoteEmu::PadMapping[i].Cc.X);
		iniFile.Set(SectionName.c_str(), "CcY", WiiMoteEmu::PadMapping[i].Cc.Y);	
		iniFile.Set(SectionName.c_str(), "CcP", WiiMoteEmu::PadMapping[i].Cc.P);
		iniFile.Set(SectionName.c_str(), "CcM", WiiMoteEmu::PadMapping[i].Cc.M);
		iniFile.Set(SectionName.c_str(), "CcH", WiiMoteEmu::PadMapping[i].Cc.H);
		iniFile.Set(SectionName.c_str(), "CcTl", WiiMoteEmu::PadMapping[i].Cc.Tl);
		iniFile.Set(SectionName.c_str(), "CcZl", WiiMoteEmu::PadMapping[i].Cc.Zl);
		iniFile.Set(SectionName.c_str(), "CcZr", WiiMoteEmu::PadMapping[i].Cc.Zr);
		iniFile.Set(SectionName.c_str(), "CcTr", WiiMoteEmu::PadMapping[i].Cc.Tr);
		iniFile.Set(SectionName.c_str(), "CcDl", WiiMoteEmu::PadMapping[i].Cc.Dl);
		iniFile.Set(SectionName.c_str(), "CcDu", WiiMoteEmu::PadMapping[i].Cc.Du);
		iniFile.Set(SectionName.c_str(), "CcDr", WiiMoteEmu::PadMapping[i].Cc.Dr);
		iniFile.Set(SectionName.c_str(), "CcDd", WiiMoteEmu::PadMapping[i].Cc.Dd);
		iniFile.Set(SectionName.c_str(), "CcLl", WiiMoteEmu::PadMapping[i].Cc.Ll);
		iniFile.Set(SectionName.c_str(), "CcLu", WiiMoteEmu::PadMapping[i].Cc.Lu);
		iniFile.Set(SectionName.c_str(), "CcLr", WiiMoteEmu::PadMapping[i].Cc.Lr);
		iniFile.Set(SectionName.c_str(), "CcLd", WiiMoteEmu::PadMapping[i].Cc.Ld);
		iniFile.Set(SectionName.c_str(), "CcRl", WiiMoteEmu::PadMapping[i].Cc.Rl);
		iniFile.Set(SectionName.c_str(), "CcRu", WiiMoteEmu::PadMapping[i].Cc.Ru);
		iniFile.Set(SectionName.c_str(), "CcRr", WiiMoteEmu::PadMapping[i].Cc.Rr);
		iniFile.Set(SectionName.c_str(), "CcRd", WiiMoteEmu::PadMapping[i].Cc.Rd);

		// Save the physical device ID number
		iniFile.Set(SectionName.c_str(), "DeviceID", WiiMoteEmu::PadMapping[i].ID);
		// ===================

		// ==================================================================
		// Joypad specific settings
		// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
			// Current joypad device ID: PadMapping[i].ID
			// Current joypad name: joyinfo[PadMapping[i].ID].Name	

		/* Save joypad specific settings. Check for "PadMapping[i].ID < SDL_NumJoysticks()" to
		   avoid reading a joyinfo that does't exist */
		if(WiiMoteEmu::PadMapping[i].ID >= WiiMoteEmu::joyinfo.size()) continue;

		// Create a new section name after the joypad name
		SectionName = WiiMoteEmu::joyinfo[WiiMoteEmu::PadMapping[i].ID].Name;

		iniFile.Set(SectionName.c_str(), "left_x", WiiMoteEmu::PadMapping[i].Axis.Lx);
		iniFile.Set(SectionName.c_str(), "left_y", WiiMoteEmu::PadMapping[i].Axis.Ly);
		iniFile.Set(SectionName.c_str(), "right_x", WiiMoteEmu::PadMapping[i].Axis.Rx);
		iniFile.Set(SectionName.c_str(), "right_y", WiiMoteEmu::PadMapping[i].Axis.Ry);
		iniFile.Set(SectionName.c_str(), "l_trigger", WiiMoteEmu::PadMapping[i].Axis.Tl);
		iniFile.Set(SectionName.c_str(), "r_trigger", WiiMoteEmu::PadMapping[i].Axis.Tr);

		iniFile.Set(SectionName.c_str(), "DeadZoneL", WiiMoteEmu::PadMapping[i].DeadZoneL);	
		iniFile.Set(SectionName.c_str(), "DeadZoneR", WiiMoteEmu::PadMapping[i].DeadZoneR);
		//iniFile.Set(SectionName.c_str(), "controllertype", WiiMoteEmu::PadMapping[i].controllertype);
		iniFile.Set(SectionName.c_str(), "TriggerType", WiiMoteEmu::PadMapping[i].triggertype);
		iniFile.Set(SectionName.c_str(), "Diagonal", WiiMoteEmu::PadMapping[i].SDiagonal);
		iniFile.Set(SectionName.c_str(), "Circle2Square", WiiMoteEmu::PadMapping[i].bCircle2Square);
		iniFile.Set(SectionName.c_str(), "RollInvert", WiiMoteEmu::PadMapping[i].bRollInvert);
		iniFile.Set(SectionName.c_str(), "PitchInvert", WiiMoteEmu::PadMapping[i].bPitchInvert);
		// ======================================
	}

    iniFile.Save(FULL_CONFIG_DIR "Wiimote.ini");
	Console::Print("Save()\n");
}
