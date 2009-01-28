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

Config g_Config;

Config::Config()
{
    memset(this, 0, sizeof(Config));
}

void Config::Load()
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

    iniFile.Save(FULL_CONFIG_DIR "Wiimote.ini");
}
