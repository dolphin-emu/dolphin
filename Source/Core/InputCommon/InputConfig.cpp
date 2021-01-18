// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Common/FileSearch.h"
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

InputConfig::InputConfig(const std::string& ini_name, const std::string& gui_name,
                         const std::string& profile_name)
    : m_ini_name(ini_name), m_gui_name(gui_name), m_profile_name(profile_name)
{
}

InputConfig::~InputConfig() = default;

void InputConfig::LoadProfiles()
{
  m_device_profile_container.Load(m_profile_name);
}

bool InputConfig::LoadConfig(bool isGC)
{
  IniFile inifile;
  static constexpr std::array<std::string_view, MAX_BBMOTES> num = {"1", "2", "3", "4", "BB"};
  std::string path;

#if defined(ANDROID)
  bool use_ir_config = false;
  std::string ir_values[3];
#endif

  m_dynamic_input_tex_config_manager.Load();

  LoadProfiles();

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
      if (static_cast<std::size_t>(i + 1) > m_controllers.size())
      {
        continue;
      }
      const auto profile_name = fmt::format("{}Profile{}", type, num[i]);
      m_controllers[i]->GetProfileManager().ClearProfileFilter();
      std::string profile_setting;
      if (control_section->Get(profile_name, &profile_setting))
      {
        const auto& setting_choices = SplitString(profile_setting, ',');
        std::vector<std::string> profiles;
        for (const std::string& setting_choice : setting_choices)
        {
          const std::string full_path =
              File::GetUserPath(D_CONFIG_IDX) + path + std::string(StripSpaces(setting_choice));
          if (File::IsDirectory(full_path))
          {
            const auto files_under_directory = Common::DoFileSearch({full_path}, {".ini"}, true);
            profiles.insert(profiles.end(), files_under_directory.begin(),
                            files_under_directory.end());
          }
          else
          {
            const std::string profile_file_path = full_path + ".ini";
            if (File::Exists(profile_file_path))
            {
              profiles.push_back(profile_file_path);
            }
          }
        }

        if (profiles.empty())
        {
          // TODO: PanicAlert shouldn't be used for this.
          PanicAlertFmtT("No profiles found for game setting '{0}'", profile_setting);
          continue;
        }

        for (auto profile : profiles)
        {
          m_controllers[i]->GetProfileManager().AddToProfileFilter(profile);
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

  if (inifile.Load(File::GetUserPath(D_CONFIG_IDX) + m_ini_name + ".ini") &&
      !inifile.GetSections().empty())
  {
    int n = 0;
    for (auto& controller : m_controllers)
    {
      IniFile::Section config;
      // Load settings from ini
      if (controller->GetProfileManager().ProfilesInFilter() > 0)
      {
        controller->GetProfileManager().SetProfileForGame(0);
        auto profile = controller->GetProfileManager().GetProfile();
        config = *profile->GetProfileData();
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

InputCommon::DeviceProfileContainer& InputConfig::GetDeviceProfileContainer()
{
  return m_device_profile_container;
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

void InputConfig::OnControllerCreated(ControllerEmu::EmulatedController& controller)
{
  controller.SetDynamicInputTextureManager(&m_dynamic_input_tex_config_manager);
  controller.GetProfileManager().SetDeviceProfileContainer(&m_device_profile_container);
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
