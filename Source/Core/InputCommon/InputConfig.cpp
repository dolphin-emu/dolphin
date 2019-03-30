// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/InputProfile.h"

InputConfig::InputConfig(const std::string& ini_name, const std::string& gui_name,
                         const std::string& profile_name)
    : m_ini_name(ini_name), m_gui_name(gui_name), m_profile_name(profile_name)
{
}

InputConfig::~InputConfig() = default;

bool InputConfig::LoadConfig(bool isGC)
{
  IniFile inifile;
  if (inifile.Load(File::GetUserPath(D_CONFIG_IDX) + m_ini_name + ".ini"))
  {
    int n = 0;
    for (auto& controller : m_controllers)
    {
      if (isGC)
      {
        if (g_profile_manager.GetGCDeviceProfileManager(n)->SetGameProfile(0))
        {
          n++;
          continue;
        }
      }
      else
      {
        if (g_profile_manager.GetWiiDeviceProfileManager(n)->SetGameProfile(0))
        {
          n++;
          continue;
        }
      }

      if (isGC)
      {
        g_profile_manager.GetGCDeviceProfileManager(n)->SetDefault();
      }
      else
      {
        g_profile_manager.GetWiiDeviceProfileManager(n)->SetDefault();
      }
      IniFile::Section config;
      config = *inifile.GetOrCreateSection(controller->GetName());
      controller->LoadConfig(&config);
      controller->UpdateReferences(g_controller_interface);

      // Next profile
      n++;
    }
    return true;
  }
  else
  {
    if (isGC)
    {
      g_profile_manager.GetGCDeviceProfileManager(0)->SetDefault();
    }
    else
    {
      g_profile_manager.GetWiiDeviceProfileManager(0)->SetDefault();
    }
    m_controllers[0]->LoadDefaults(g_controller_interface);
    m_controllers[0]->UpdateReferences(g_controller_interface);
    return false;
  }
}

void InputConfig::SaveConfig()
{
  std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + m_ini_name + ".ini";

  IniFile inifile;
  inifile.Load(ini_filename);

  for (auto& controller : m_controllers)
    controller->SaveConfig(inifile.GetOrCreateSection(controller->GetName()));

  inifile.Save(ini_filename);
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

std::size_t InputConfig::GetControllerCount() const
{
  return m_controllers.size();
}

void InputConfig::RegisterHotplugCallback()
{
  // Update control references on all controllers
  // as configured devices may have been added or removed.
  m_hotplug_callback_handle = g_controller_interface.RegisterDevicesChangedCallback([this] {
    for (auto& controller : m_controllers)
      controller->UpdateReferences(g_controller_interface);
  });
}

void InputConfig::UnregisterHotplugCallback()
{
  g_controller_interface.UnregisterDevicesChangedCallback(m_hotplug_callback_handle);
}

bool InputConfig::IsControllerControlledByGamepadDevice(int index) const
{
  if (static_cast<size_t>(index) >= m_controllers.size())
    return false;

  const auto& controller = m_controllers.at(index).get()->GetDefaultDevice();

  // Filter out anything which obviously not a gamepad
  return !((controller.source == "Quartz")      // OSX Quartz Keyboard/Mouse
           || (controller.source == "XInput2")  // Linux and BSD Keyboard/Mouse
           || (controller.source == "Android" &&
               controller.name == "Touchscreen")  // Android Touchscreen
           || (controller.source == "DInput" &&
               controller.name == "Keyboard Mouse"));  // Windows Keyboard/Mouse
}
