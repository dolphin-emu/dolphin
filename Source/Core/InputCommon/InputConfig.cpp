// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/InputConfig.h"

#include <vector>

#include "Common/Config/Config.h"
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
#include "InputCommon/InputProfile.h"

InputConfig::InputConfig(const std::string& ini_name, const std::string& gui_name,
                         const std::string& profile_name)
    : m_ini_name(ini_name), m_gui_name(gui_name), m_profile_name(profile_name)
{
}

InputConfig::~InputConfig() = default;

bool InputConfig::LoadConfig(InputClass type)
{
  Common::IniFile inifile;
  bool useProfile[MAX_BBMOTES] = {false, false, false, false, false};
  static constexpr std::array<std::string_view, MAX_BBMOTES> num = {"1", "2", "3", "4", "BB"};
  std::string profile[MAX_BBMOTES];
  std::string path;

  m_dynamic_input_tex_config_manager.Load();

  if (SConfig::GetInstance().GetGameID() != "00000000")
  {
    std::string type_str;
    switch (type)
    {
    case InputClass::GBA:
      type_str = "GBA";
      path = "Profiles/GBA/";
      break;
    case InputClass::Wii:
      type_str = "Wiimote";
      path = "Profiles/Wiimote/";
      break;
    case InputClass::GC:
    default:
      type_str = "Pad";
      path = "Profiles/GCPad/";
      break;
    }

    Common::IniFile game_ini = SConfig::GetInstance().LoadGameIni();
    auto* control_section = game_ini.GetOrCreateSection("Controls");

    for (int i = 0; i < 4; i++)
    {
      const auto profile_name = fmt::format("{}Profile{}", type_str, num[i]);

      if (control_section->Exists(profile_name))
      {
        std::string profile_setting;
        if (control_section->Get(profile_name, &profile_setting))
        {
          auto profiles = InputProfile::GetProfilesFromSetting(
              profile_setting, File::GetUserPath(D_CONFIG_IDX) + path);

          if (profiles.empty())
          {
            // TODO: PanicAlert shouldn't be used for this.
            PanicAlertFmtT("No profiles found for game setting '{0}'", profile_setting);
            continue;
          }

          // Use the first profile by default
          profile[i] = profiles[0];
          useProfile[i] = true;
        }
      }
    }
  }

  if (inifile.Load(File::GetUserPath(D_CONFIG_IDX) + m_ini_name + ".ini") &&
      !inifile.GetSections().empty())
  {
    int n = 0;

    std::vector<std::string> controller_names;
    for (auto& controller : m_controllers)
    {
      Common::IniFile::Section config;
      // Load settings from ini
      if (useProfile[n])
      {
        std::string base;
        SplitPath(profile[n], nullptr, &base, nullptr);
        Core::DisplayMessage("Loading game specific input profile '" + base + "' for device '" +
                                 controller->GetName() + "'",
                             6000);

        inifile.Load(profile[n]);
        config = *inifile.GetOrCreateSection("Profile");
      }
      else
      {
        config = *inifile.GetOrCreateSection(controller->GetName());
      }
      controller->LoadConfig(&config);
      controller->UpdateReferences(g_controller_interface);
      controller_names.push_back(controller->GetName());

      // Next profile
      n++;
    }

    m_dynamic_input_tex_config_manager.GenerateTextures(inifile, controller_names);
    return true;
  }
  else
  {
    // Only load the default profile for the first controller and clear the others,
    // otherwise they would all share the same mappings on the same (default) device
    if (m_controllers.size() > 0)
    {
      m_controllers[0]->LoadDefaults(g_controller_interface);
      m_controllers[0]->UpdateReferences(g_controller_interface);
    }
    for (size_t i = 1; i < m_controllers.size(); ++i)
    {
      // Calling the base version just clears all settings without overwriting them with a default
      m_controllers[i]->EmulatedController::LoadDefaults(g_controller_interface);
      m_controllers[i]->UpdateReferences(g_controller_interface);
    }
    return false;
  }
}

void InputConfig::SaveConfig()
{
  std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + m_ini_name + ".ini";

  Common::IniFile inifile;
  inifile.Load(ini_filename);

  std::vector<std::string> controller_names;
  for (auto& controller : m_controllers)
  {
    controller->SaveConfig(inifile.GetOrCreateSection(controller->GetName()));
    controller_names.push_back(controller->GetName());
  }

  m_dynamic_input_tex_config_manager.GenerateTextures(inifile, controller_names);

  inifile.Save(ini_filename);
}

ControllerEmu::EmulatedController* InputConfig::GetController(int index) const
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

int InputConfig::GetControllerCount() const
{
  return static_cast<int>(m_controllers.size());
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

void InputConfig::GenerateControllerTextures(const Common::IniFile& file)
{
  std::vector<std::string> controller_names;
  for (auto& controller : m_controllers)
  {
    controller_names.push_back(controller->GetName());
  }

  m_dynamic_input_tex_config_manager.GenerateTextures(file, controller_names);
}
