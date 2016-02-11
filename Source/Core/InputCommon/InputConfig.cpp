// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Common/FileUtil.h"
#include "Common/OnionConfig.h"

#include "Core/ConfigManager.h"
#include "Core/HW/Wiimote.h"
#include "InputCommon/ControllerEmu.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

bool InputConfig::LoadConfig(bool isGC)
{
	m_controllers[0]->LoadDefaults(g_controller_interface);
	m_controllers[0]->UpdateReferences(g_controller_interface);

	for (auto& controller : m_controllers)
	{
		OnionConfig::OnionSystem system[] =
		{
			OnionConfig::OnionSystem::SYSTEM_GCPAD,
			OnionConfig::OnionSystem::SYSTEM_WIIPAD,
		};
		controller->LoadConfig(OnionConfig::GetOrCreatePetal(system[isGC ? 1 : 0], controller->GetName()));

		// Update refs
		controller->UpdateReferences(g_controller_interface);
	}
	return true;
}

void InputConfig::SaveConfig()
{
	// XXX:
//	std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + m_ini_name + ".ini";
//
//	IniFile inifile;
//	inifile.Load(ini_filename);
//
//	for (auto& controller : m_controllers)
//		controller->SaveConfig(inifile.GetOrCreateSection(controller->GetName()));
//
//	inifile.Save(ini_filename);
}

ControllerEmu* InputConfig::GetController(int index)
{
	return m_controllers.at(index).get();
}

void InputConfig::ClearControllers()
{
	m_controllers.clear();
}

bool InputConfig::ControllersNeedToBeCreated() const
{
	return m_controllers.empty();
}
