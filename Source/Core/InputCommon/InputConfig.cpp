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

bool InputConfig::LoadConfig()
{
	m_controllers[0]->LoadDefaults(g_controller_interface);
	m_controllers[0]->UpdateReferences(g_controller_interface);

	for (auto& controller : m_controllers)
	{
		controller->LoadConfig(OnionConfig::GetOrCreatePetal(m_system, controller->GetName()));

		// Update refs
		controller->UpdateReferences(g_controller_interface);
	}
	return true;
}

void InputConfig::SaveConfig()
{
	for (auto& controller : m_controllers)
		controller->SaveConfig(OnionConfig::GetOrCreatePetal(m_system, controller->GetName()));
	OnionConfig::Save();
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
