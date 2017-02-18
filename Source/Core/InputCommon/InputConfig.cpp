// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Common/Config.h"
#include "Common/FileUtil.h"

#include "Core/ConfigManager.h"
#include "Core/HW/Wiimote.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"

InputConfig::InputConfig(const Config::System system, const std::string& gui_name,
                         const std::string& profile_name)
    : m_system(system), m_gui_name(gui_name), m_profile_name(profile_name)
{
}

InputConfig::~InputConfig() = default;

bool InputConfig::LoadConfig()
{
  for (auto& controller : m_controllers)
  {
    controller->LoadConfig(Config::GetOrCreateSection(m_system, controller->GetName()));

    // Update refs
    controller->UpdateReferences(g_controller_interface);
  }
  return true;
}

void InputConfig::SaveConfig()
{
  for (auto& controller : m_controllers)
    controller->SaveConfig(Config::GetOrCreateSection(m_system, controller->GetName()));
  Config::Save();
}

ControllerEmu::EmulatedController* InputConfig::GetController(int index)
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

bool InputConfig::IsControllerControlledByGamepadDevice(int index) const
{
  if (static_cast<size_t>(index) >= m_controllers.size())
    return false;

  const auto& controller = m_controllers.at(index).get()->default_device;

  // Filter out anything which obviously not a gamepad
  return !((controller.source == "Keyboard")    // OSX IOKit Keyboard/Mouse
           || (controller.source == "Quartz")   // OSX Quartz Keyboard/Mouse
           || (controller.source == "XInput2")  // Linux and BSD Keyboard/Mouse
           || (controller.source == "Android" &&
               controller.name == "Touchscreen")  // Android Touchscreen
           || (controller.source == "DInput" &&
               controller.name == "Keyboard Mouse"));  // Windows Keyboard/Mouse
}
