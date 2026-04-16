// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/InputConfig.h"

#include <array>
#include <optional>
#include <vector>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputProfile.h"
#include "InputCommon/TriforcePadProfile.h"

namespace
{

struct ProfileSource
{
  std::string_view profile_key;
  std::string_view profile_directory_name;
  bool is_triforce_profile;
};

struct SelectedProfile
{
  std::string path;
  bool is_triforce_profile = false;
};

std::string GetUserProfileDirectoryPathForNamespace(std::string_view profile_directory_name)
{
  return File::GetUserPath(D_CONFIG_IDX) + "Profiles/" + std::string(profile_directory_name) + "/";
}

std::vector<ProfileSource> GetGameProfileSources(const InputConfig& config)
{
  std::vector<ProfileSource> profile_sources;

  if (config.GetProfileKey() == "Pad")
  {
    // Triforce reuses the GC pad backend, but it gets its own profile namespace in game INIs.
    profile_sources.push_back({"TriforcePad", "TriforcePad", true});
  }

  profile_sources.push_back({config.GetProfileKey(), config.GetProfileDirectoryName(), false});
  return profile_sources;
}

std::optional<SelectedProfile> FindGameProfileForPort(const InputConfig& config,
                                                      const Common::IniFile::Section& controls,
                                                      std::string_view port_suffix)
{
  for (const ProfileSource& profile_source : GetGameProfileSources(config))
  {
    // Triforce shares the GC pad backend, but game INIs may opt into the
    // TriforcePad namespace to load family-aware profile sections instead.
    const auto profile_name = fmt::format("{}Profile{}", profile_source.profile_key, port_suffix);
    if (!controls.Exists(profile_name))
      continue;

    std::string profile_setting;
    if (!controls.Get(profile_name, &profile_setting))
      return std::nullopt;

    const std::string profile_directory =
        GetUserProfileDirectoryPathForNamespace(profile_source.profile_directory_name);
    auto profiles = InputProfile::GetProfilesFromSetting(profile_setting, profile_directory);

    if (profiles.empty())
    {
      // TODO: PanicAlert shouldn't be used for this.
      PanicAlertFmtT("No profiles found for game setting '{0}'", profile_setting);
      return std::nullopt;
    }

    return SelectedProfile{profiles[0], profile_source.is_triforce_profile};
  }

  return std::nullopt;
}

const Common::IniFile::Section* GetProfileSectionForLoad(const Common::IniFile& ini,
                                                         bool is_triforce_profile)
{
  return is_triforce_profile ? TriforcePadProfile::GetEffectiveProfileSection(ini) :
                               ini.GetSection("Profile");
}

}  // namespace

InputConfig::InputConfig(const std::string& ini_name, const std::string& gui_name,
                         const std::string& profile_directory_name, const std::string& profile_key)
    : m_ini_name(ini_name), m_gui_name(gui_name), m_profile_directory_name(profile_directory_name),
      m_profile_key(profile_key)
{
}

InputConfig::~InputConfig() = default;

bool InputConfig::LoadConfig()
{
  Common::IniFile inifile;
  static constexpr std::array<std::string_view, MAX_BBMOTES> num = {"1", "2", "3", "4", "BB"};
  std::array<std::optional<SelectedProfile>, MAX_BBMOTES> selected_profiles;

  if (SConfig::GetInstance().GetGameID() != "00000000")
  {
    Common::IniFile game_ini = SConfig::GetInstance().LoadGameIni();
    Common::IniFile::Section* const control_section = game_ini.GetOrCreateSection("Controls");

    for (int i = 0; i < 4; ++i)
      selected_profiles[i] = FindGameProfileForPort(*this, *control_section, num[i]);
  }

  if (inifile.Load(File::GetUserPath(D_CONFIG_IDX) + m_ini_name + ".ini") &&
      !inifile.GetSections().empty())
  {
    int controller_index = 0;

    for (auto& controller : m_controllers)
    {
      Common::IniFile::Section config;
      const std::optional<SelectedProfile>& selected_profile = selected_profiles[controller_index];
      if (selected_profile.has_value())
      {
        std::string base;
        SplitPath(selected_profile->path, nullptr, &base, nullptr);
        Core::DisplayMessage("Loading game specific input profile '" + base + "' for device '" +
                                 controller->GetName() + "'",
                             6000);

        inifile.Load(selected_profile->path);
        if (const Common::IniFile::Section* const section =
                GetProfileSectionForLoad(inifile, selected_profile->is_triforce_profile))
        {
          config = *section;
        }
      }
      else
      {
        config = *inifile.GetOrCreateSection(controller->GetName());
      }
      controller->LoadConfig(&config);
      controller->UpdateReferences(g_controller_interface);

      // Next profile
      ++controller_index;
    }
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

  for (auto& controller : m_controllers)
  {
    controller->SaveConfig(inifile.GetOrCreateSection(controller->GetName()));
  }

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

std::string InputConfig::GetUserProfileDirectoryPath() const
{
  return fmt::format("{}Profiles/{}/", File::GetUserPath(D_CONFIG_IDX), GetProfileDirectoryName());
}

std::string InputConfig::GetSysProfileDirectoryPath() const
{
  return fmt::format("{}Profiles/{}/", File::GetSysDirectory(), GetProfileDirectoryName());
}

int InputConfig::GetControllerCount() const
{
  return static_cast<int>(m_controllers.size());
}

void InputConfig::RegisterHotplugCallback()
{
  // Update control references on all controllers
  // as configured devices may have been added or removed.
  m_hotplug_event_hook = g_controller_interface.RegisterDevicesChangedCallback([this] {
    for (auto& controller : m_controllers)
      controller->UpdateReferences(g_controller_interface);
  });
}

void InputConfig::UnregisterHotplugCallback()
{
  m_hotplug_event_hook.reset();
}

bool InputConfig::IsControllerControlledByGamepadDevice(int index) const
{
  if (static_cast<size_t>(index) >= m_controllers.size())
    return false;

  const auto& controller = m_controllers.at(index).get()->GetDefaultDevice();

  // By default on Android, no device is selected
  if (controller.source == "")
    return false;

  // Filter out anything which obviously not a gamepad
  return !((controller.source == "Quartz")      // OSX Quartz Keyboard/Mouse
           || (controller.source == "XInput2")  // Linux and BSD Keyboard/Mouse
           || (controller.source == "Android" && controller.cid <= 0)  // Android non-gamepad device
           || (controller.source == "DInput" &&
               controller.name == "Keyboard Mouse"));  // Windows Keyboard/Mouse
}

void InputConfig::GenerateControllerTextures(const Common::IniFile& file)
{
  m_dynamic_input_tex_config_manager.Load();

  std::vector<std::string> controller_names;
  for (auto& controller : m_controllers)
  {
    controller_names.push_back(controller->GetName());
  }

  m_dynamic_input_tex_config_manager.GenerateTextures(file, controller_names);
}

void InputConfig::GenerateControllerTextures()
{
  const std::string ini_filename = File::GetUserPath(D_CONFIG_IDX) + m_ini_name + ".ini";

  Common::IniFile inifile;
  inifile.Load(ini_filename);

  for (auto& controller : m_controllers)
  {
    controller->SaveConfig(inifile.GetOrCreateSection(controller->GetName()));
  }

  GenerateControllerTextures(inifile);
}
