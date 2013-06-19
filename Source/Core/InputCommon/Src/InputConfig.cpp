// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "InputConfig.h"
#include "../../Core/Src/ConfigManager.h"
#include "../../Core/Src/HW/Wiimote.h"

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
	bool useProfile[MAX_BBMOTES] = {false, false, false, false, false};
	std::string num[MAX_BBMOTES] = {"1", "2", "3", "4", "BB"};
	std::string profile[MAX_BBMOTES];
	std::string path;

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID() != "00000000")
	{
		std::string type;
		if (isGC)
		{
			type = "Pad";
			path = "Profiles/GCPad/";
		}
		else
		{
			type = "Wiimote";
			path = "Profiles/Wiimote/";
		}
		game_ini.Load(File::GetUserPath(D_GAMECONFIG_IDX) + SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID() + ".ini");
		for (int i = 0; i < 4; i++)
		{
			if (game_ini.Exists("Controls", (type + "Profile" + num[i]).c_str()))
			{
				game_ini.Get("Controls", (type + "Profile" + num[i]).c_str(), &profile[i]);
				if (File::Exists(File::GetUserPath(D_CONFIG_IDX) + path + profile[i] + ".ini"))
					useProfile[i] = true;
				else
					PanicAlertT("Selected controller profile does not exist");
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
				profile_ini.Load(File::GetUserPath(D_CONFIG_IDX) + path + profile[n] + ".ini");
				(*i)->LoadConfig(profile_ini.GetOrCreateSection("Profile"));
			}
			else
			{
				(*i)->LoadConfig(inifile.GetOrCreateSection((*i)->GetName().c_str()));
			}

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
