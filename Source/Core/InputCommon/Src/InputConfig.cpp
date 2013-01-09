// Copyright (C) 2010 Dolphin Project.

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

#include "InputConfig.h"
#include "../../Core/Src/ConfigManager.h"

InputPlugin::~InputPlugin()
{
	// delete pads
	std::vector<ControllerEmu*>::const_iterator i = controllers.begin(),
		e = controllers.end();
	for ( ; i != e; ++i )
		delete *i;
}

bool InputPlugin::LoadConfig(bool isGC)
{
	IniFile inifile;
	IniFile game_ini;
	bool useProfile[4] = {false, false, false, false};
	std::string num[4] = {"1", "2", "3", "4"};
	std::string profile[4];

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID() != "00000000")
	{
		std::string type;
		if (isGC)
			type = "GC";
		else
			type = "Wii";
		game_ini.Load(File::GetUserPath(D_GAMECONFIG_IDX) + SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID() + ".ini");
		for (int i = 0; i < 4; i++)
		{
			if (game_ini.Exists("Core", (type + "Profile" + num[i]).c_str()))
			{
				useProfile[i] = true;
				game_ini.Get("Core", (type + "Profile" + num[i]).c_str(), &profile[i]);
			}
		}
	}

	if (inifile.Load(File::GetUserPath(D_CONFIG_IDX) + ini_name + ".ini"))
	{
		std::vector< ControllerEmu* >::const_iterator
			i = controllers.begin(),
			e = controllers.end();
		for (int n = 0; i!=e; ++i, ++n)
		{
			// load settings from ini
			if (useProfile[n])
			{
				IniFile profile_ini;
				std::string path;
				if (isGC)
					path = "Profiles/GCPad/";
				else
					path = "Profiles/Wiimote/";
				profile_ini.Load(File::GetUserPath(D_CONFIG_IDX) + path + profile[n] + ".ini");
				(*i)->LoadConfig(profile_ini.GetOrCreateSection("Profile"));
			}
			else
				(*i)->LoadConfig(inifile.GetOrCreateSection((*i)->GetName().c_str()));
			// update refs
			(*i)->UpdateReferences(g_controller_interface);
		}
		return true;
	}
	else
	{
		controllers[0]->LoadDefaults(g_controller_interface);
		controllers[0]->UpdateReferences(g_controller_interface);
		return false;
	}
}

void InputPlugin::SaveConfig()
{
	std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + ini_name + ".ini";

	IniFile inifile;
	inifile.Load(ini_filename);

	std::vector< ControllerEmu* >::const_iterator i = controllers.begin(),
		e = controllers.end();
	for ( ; i!=e; ++i )
		(*i)->SaveConfig(inifile.GetOrCreateSection((*i)->GetName().c_str()));
	
	inifile.Save(ini_filename);
}
