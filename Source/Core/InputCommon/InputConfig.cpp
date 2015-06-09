// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Wiimote.h"
#include "InputCommon/InputConfig.h"

bool InputConfig::LoadConfig(bool isGC)
{
	IniFile inifile;
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

		IniFile game_ini = SConfig::GetInstance().m_LocalCoreStartupParameter.LoadGameIni();
		IniFile::Section* control_section = game_ini.GetOrCreateSection("Controls");

		for (int i = 0; i < 4; i++)
		{
			if (control_section->Exists(type + "Profile" + num[i]))
			{
				if (control_section->Get(type + "Profile" + num[i], &profile[i]))
				{
					if (File::Exists(File::GetUserPath(D_CONFIG_IDX) + path + profile[i] + ".ini"))
					{
						useProfile[i] = true;
					}
					else
					{
						// TODO: PanicAlert shouldn't be used for this.
						PanicAlertT("Selected controller profile does not exist");
					}
				}
			}
		}
	}

	if (inifile.Load(File::GetUserPath(D_CONFIG_IDX) + ini_name + ".ini"))
	{
		int n = 0;
		for (ControllerEmu* pad : controllers)
		{
			// Load settings from ini
			if (useProfile[n])
			{
				IniFile profile_ini;
				profile_ini.Load(File::GetUserPath(D_CONFIG_IDX) + path + profile[n] + ".ini");
				pad->LoadConfig(profile_ini.GetOrCreateSection("Profile"));
			}
			else
			{
				pad->LoadConfig(inifile.GetOrCreateSection(pad->GetName()));
			}

			// Update refs
			pad->UpdateReferences(g_controller_interface);

			// Next profile
			n++;
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

void InputConfig::SaveConfig()
{
	std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + ini_name + ".ini";

	IniFile inifile;
	inifile.Load(ini_filename);

	for (ControllerEmu* pad : controllers)
		pad->SaveConfig(inifile.GetOrCreateSection(pad->GetName()));

	inifile.Save(ini_filename);
}
