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
  bool useProfile[MAX_BBMOTES] = {false, false, false, false, false};
  std::string num[MAX_BBMOTES] = {"1", "2", "3", "4", "BB"};
  std::string profile[MAX_BBMOTES];
  std::string path;

#if defined(ANDROID)
  bool use_ir_config = false;
  std::string ir_values[3];
#endif

  if (SConfig::GetInstance().GetGameID() != "00000000")
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

    IniFile game_ini = SConfig::GetInstance().LoadGameIni();
    IniFile::Section* control_section = game_ini.GetOrCreateSection("Controls");

    for (int i = 0; i < 4; i++)
    {
      if (control_section->Exists(type + "Profile" + num[i]))
      {
        std::string profile_setting;
        if (control_section->Get(type + "Profile" + num[i], &profile_setting))
        {
          auto profiles = InputProfile::GetProfilesFromSetting(
              profile_setting, File::GetUserPath(D_CONFIG_IDX) + path);

          if (profiles.empty())
          {
            // TODO: PanicAlert shouldn't be used for this.
            PanicAlertT("No profiles found for game setting '%s'", profile_setting.c_str());
            continue;
          }

          // Use the first profile by default
          profile[i] = profiles[0];
          useProfile[i] = true;
        }
      }
    }
#if defined(ANDROID)
    // For use on android touchscreen IR pointer
    // Check for IR values
    if (control_section->Exists("IRTotalYaw") && control_section->Exists("IRTotalPitch") &&
        control_section->Exists("IRVerticalOffset"))
    {
      use_ir_config = true;
      control_section->Get("IRTotalYaw", &ir_values[0]);
      control_section->Get("IRTotalPitch", &ir_values[1]);
      control_section->Get("IRVerticalOffset", &ir_values[2]);
    }
#endif
  }

  if (inifile.Load(File::GetUserPath(D_CONFIG_IDX) + m_ini_name + ".ini"))
  {
    int n = 0;
    for (auto& controller : m_controllers)
    {
      IniFile::Section config;
      // Load settings from ini
      if (useProfile[n])
      {
        std::string base;
        SplitPath(profile[n], nullptr, &base, nullptr);
        Core::DisplayMessage("Loading game specific input profile '" + base + "' for device '" +
                                 controller->GetName() + "'",
                             6000);

        IniFile profile_ini;
        profile_ini.Load(profile[n]);
        config = *profile_ini.GetOrCreateSection("Profile");
      }
      else
      {
        config = *inifile.GetOrCreateSection(controller->GetName());
      }
#if defined(ANDROID)
      // Only set for wii pads
      if (!isGC && use_ir_config)
      {
        config.Set("IR/Total Yaw", ir_values[0]);
        config.Set("IR/Total Pitch", ir_values[1]);
        config.Set("IR/Vertical Offset", ir_values[2]);
      }
#endif
      controller->LoadConfig(&config);
      // Update refs
      controller->UpdateReferences(g_controller_interface);

      // Next profile
      n++;
    }
    return true;
  }
  else
  {
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
