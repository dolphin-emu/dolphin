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

		// Don't update this when we are loading settings from the ConfigBox
		if(!ChangePad)
		{
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
		if(WiiMoteEmu::PadMapping[i].ID >= SDL_NumJoysticks()) continue;

		// Create a section name			
		SectionName = WiiMoteEmu::joyinfo[WiiMoteEmu::PadMapping[i].ID].Name;

		iniFile.Get(SectionName.c_str(), "left_x", &WiiMoteEmu::PadMapping[i].Axis.Lx, 0);	
		iniFile.Get(SectionName.c_str(), "left_y", &WiiMoteEmu::PadMapping[i].Axis.Ly, 1);	
		iniFile.Get(SectionName.c_str(), "right_x", &WiiMoteEmu::PadMapping[i].Axis.Rx, 2);	
		iniFile.Get(SectionName.c_str(), "right_y", &WiiMoteEmu::PadMapping[i].Axis.Ry, 3);
		iniFile.Get(SectionName.c_str(), "l_trigger", &WiiMoteEmu::PadMapping[i].Axis.Tl, 4);
		iniFile.Get(SectionName.c_str(), "r_trigger", &WiiMoteEmu::PadMapping[i].Axis.Tr, 5);
		iniFile.Get(SectionName.c_str(), "DeadZone", &WiiMoteEmu::PadMapping[i].deadzone, 0);
		iniFile.Get(SectionName.c_str(), "TriggerType", &WiiMoteEmu::PadMapping[i].triggertype, 0);
		iniFile.Get(SectionName.c_str(), "Diagonal", &WiiMoteEmu::PadMapping[i].SDiagonal, "100%");
		iniFile.Get(SectionName.c_str(), "SquareToCircle", &WiiMoteEmu::PadMapping[i].bSquareToCircle, false);
		iniFile.Get(SectionName.c_str(), "NoTriggerFilter", &WiiMoteEmu::PadMapping[i].bNoTriggerFilter, false);
	}
	// =============================
	Console::Print("Load()\n");
}

void Config::Save()
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
		if(WiiMoteEmu::PadMapping[i].ID >= SDL_NumJoysticks()) continue;

		// Create a new section name after the joypad name
		SectionName = WiiMoteEmu::joyinfo[WiiMoteEmu::PadMapping[i].ID].Name;

		iniFile.Set(SectionName.c_str(), "left_x", WiiMoteEmu::PadMapping[i].Axis.Lx);
		iniFile.Set(SectionName.c_str(), "left_y", WiiMoteEmu::PadMapping[i].Axis.Ly);
		iniFile.Set(SectionName.c_str(), "right_x", WiiMoteEmu::PadMapping[i].Axis.Rx);
		iniFile.Set(SectionName.c_str(), "right_y", WiiMoteEmu::PadMapping[i].Axis.Ry);
		iniFile.Set(SectionName.c_str(), "l_trigger", WiiMoteEmu::PadMapping[i].Axis.Tl);
		iniFile.Set(SectionName.c_str(), "r_trigger", WiiMoteEmu::PadMapping[i].Axis.Tr);

		//iniFile.Set(SectionName.c_str(), "deadzone", PadMapping[i].deadzone);		
		//iniFile.Set(SectionName.c_str(), "controllertype", PadMapping[i].controllertype);
		iniFile.Set(SectionName.c_str(), "TriggerType", WiiMoteEmu::PadMapping[i].triggertype);
		//iniFile.Set(SectionName.c_str(), "Diagonal", PadMapping[i].SDiagonal);
		//iniFile.Set(SectionName.c_str(), "SquareToCircle", PadMapping[i].bSquareToCircle);
		iniFile.Set(SectionName.c_str(), "NoTriggerFilter", WiiMoteEmu::PadMapping[i].bNoTriggerFilter);
		// ======================================
	}

    iniFile.Save(FULL_CONFIG_DIR "Wiimote.ini");
	Console::Print("Save()\n");
}
