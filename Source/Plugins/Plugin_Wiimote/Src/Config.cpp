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


#include "Common.h"
#include "IniFile.h"

#include "Config.h"
#include "EmuDefinitions.h" // for PadMapping

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

	// get resolution

    iniFile.Get("Settings", "SidewaysDPad", &bSidewaysDPad, false); // Hardware
	iniFile.Get("Settings", "WideScreen", &bWideScreen, false);
	iniFile.Get("Settings", "NunchuckConnected", &bNunchuckConnected, false);
	iniFile.Get("Settings", "ClassicControllerConnected", &bClassicControllerConnected, false);

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
		// ==================================================================
		// Slot specific settings
		// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
		std::string SectionName = StringFromFormat("Wiimote%i", i + 1);
		iniFile.Get(SectionName.c_str(), "NoTriggerFilter", &bNoTriggerFilter, false);
		iniFile.Get(SectionName.c_str(), "TriggerType", &Trigger.Type, Trigger.TRIGGER_OFF);
		iniFile.Get(SectionName.c_str(), "TriggerRollRange", &Trigger.Range.Roll, 50);
		iniFile.Get(SectionName.c_str(), "TriggerPitchRange", &Trigger.Range.Pitch, false);

		// Wiimote
		iniFile.Get(SectionName.c_str(), "WmA", &WiiMoteEmu::PadMapping[i].Wm.A, 0);
		iniFile.Get(SectionName.c_str(), "WmB", &WiiMoteEmu::PadMapping[i].Wm.B, 0);
		iniFile.Get(SectionName.c_str(), "Wm1", &WiiMoteEmu::PadMapping[i].Wm.One, 0);
		iniFile.Get(SectionName.c_str(), "Wm2", &WiiMoteEmu::PadMapping[i].Wm.Two, 0);
		iniFile.Get(SectionName.c_str(), "WmP", &WiiMoteEmu::PadMapping[i].Wm.P, 0);
		iniFile.Get(SectionName.c_str(), "WmM", &WiiMoteEmu::PadMapping[i].Wm.M, 0);
		iniFile.Get(SectionName.c_str(), "WmH", &WiiMoteEmu::PadMapping[i].Wm.H, 0);
		iniFile.Get(SectionName.c_str(), "WmL", &WiiMoteEmu::PadMapping[i].Wm.L, 0);
		iniFile.Get(SectionName.c_str(), "WmR", &WiiMoteEmu::PadMapping[i].Wm.R, 0);
		iniFile.Get(SectionName.c_str(), "WmU", &WiiMoteEmu::PadMapping[i].Wm.U, 0);
		iniFile.Get(SectionName.c_str(), "WmD", &WiiMoteEmu::PadMapping[i].Wm.D, 0);
		iniFile.Get(SectionName.c_str(), "WmShake", &WiiMoteEmu::PadMapping[i].Wm.Shake, 0);

		// Nunchuck
		iniFile.Get(SectionName.c_str(), "NunchuckStick", &Nunchuck.Type, Nunchuck.KEYBOARD);
		iniFile.Get(SectionName.c_str(), "NcZ", &WiiMoteEmu::PadMapping[i].Nc.Z, 0);
		iniFile.Get(SectionName.c_str(), "NcC", &WiiMoteEmu::PadMapping[i].Nc.C, 0);
		iniFile.Get(SectionName.c_str(), "NcL", &WiiMoteEmu::PadMapping[i].Nc.L, 0);
		iniFile.Get(SectionName.c_str(), "NcR", &WiiMoteEmu::PadMapping[i].Nc.R, 0);	
		iniFile.Get(SectionName.c_str(), "NcU", &WiiMoteEmu::PadMapping[i].Nc.U, 0);
		iniFile.Get(SectionName.c_str(), "NcD", &WiiMoteEmu::PadMapping[i].Nc.D, 0);
		iniFile.Get(SectionName.c_str(), "NcShake", &WiiMoteEmu::PadMapping[i].Nc.Shake, 0);

		// Don't update this when we are loading settings from the ConfigBox
		if(!ChangePad)
		{
			iniFile.Get(SectionName.c_str(), "DeviceID", &WiiMoteEmu::PadMapping[i].ID, 0);
			iniFile.Get(SectionName.c_str(), "Enabled", &WiiMoteEmu::PadMapping[i].enabled, true);
		}

		// Check if the pad ID is within the range of avaliable pads
		if (WiiMoteEmu::PadMapping[i].ID > (WiiMoteEmu::NumPads - 1)) WiiMoteEmu::PadMapping[i].ID = (WiiMoteEmu::NumPads - 1);
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
    iniFile.Set("Settings", "WideScreen", bWideScreen);
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

		// Nunchuck
		iniFile.Set(SectionName.c_str(), "NunchuckStick", Nunchuck.Type);
		iniFile.Set(SectionName.c_str(), "NcZ", WiiMoteEmu::PadMapping[i].Nc.Z);
		iniFile.Set(SectionName.c_str(), "NcC", WiiMoteEmu::PadMapping[i].Nc.C);
		iniFile.Set(SectionName.c_str(), "NcL", WiiMoteEmu::PadMapping[i].Nc.L);
		iniFile.Set(SectionName.c_str(), "NcR", WiiMoteEmu::PadMapping[i].Nc.R);
		iniFile.Set(SectionName.c_str(), "NcU", WiiMoteEmu::PadMapping[i].Nc.U);
		iniFile.Set(SectionName.c_str(), "NcShake", WiiMoteEmu::PadMapping[i].Nc.Shake);

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
