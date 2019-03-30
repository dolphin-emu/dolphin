// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/Wiimote.h"
#include "Core/HotkeyManager.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/InputProfile.h"

#include <algorithm>
#include <iterator>

InputProfile::ProfileManager g_profile_manager;

namespace InputProfile
{
namespace
{
constexpr int display_message_ms = 3000;
}

std::vector<std::string> GetProfilesFromSetting(const std::string& setting, const std::string& root)
{
  const auto& setting_choices = SplitString(setting, ',');

  std::vector<std::string> result;
  for (const std::string& setting_choice : setting_choices)
  {
    const std::string path = root + StripSpaces(setting_choice);
    if (File::IsDirectory(path))
    {
      const auto files_under_directory = Common::DoFileSearch({path}, {".ini"}, true);
      result.insert(result.end(), files_under_directory.begin(), files_under_directory.end());
    }
    else
    {
      const std::string file_path = path + ".ini";
      if (File::Exists(file_path))
      {
        result.push_back(file_path);
      }
    }
  }

  return result;
}

DeviceProfileManager::DeviceProfileManager(InputConfig* device_configuration, int controller_index,
                                           std::string setting_name)
    : m_controller_index(controller_index), m_controller_setting_name(setting_name),
      m_device_configuration(device_configuration)
{
}

std::vector<std::string>
DeviceProfileManager::GetProfilesForDevice(InputConfig* device_configuration)
{
  const std::string device_profile_root_location(File::GetUserPath(D_CONFIG_IDX) + "Profiles/" +
                                                 device_configuration->GetProfileName());
  return Common::DoFileSearch({device_profile_root_location}, {".ini"}, true);
}

std::string DeviceProfileManager::GetProfile(int index, const std::vector<std::string>& profiles)
{
  // update the index and bind it to the number of available strings
  auto positive_modulo = [](int& i, int n) { i = (i % n + n) % n; };
  positive_modulo(index, static_cast<int>(profiles.size()));

  return profiles[index];
}

void DeviceProfileManager::UpdateToProfile(const std::string& profile_path,
                                           ControllerEmu::EmulatedController* controller)
{
  std::string base;
  SplitPath(profile_path, nullptr, &base, nullptr);

  IniFile ini_file;
  if (ini_file.Load(profile_path))
  {
    Core::DisplayMessage("Loading input profile '" + base + "' for device '" +
                             controller->GetName() + "'",
                         display_message_ms);
    controller->LoadConfig(ini_file.GetOrCreateSection("Profile"));
    controller->UpdateReferences(g_controller_interface);
    m_profile_path = profile_path;
  }
  else
  {
    Core::DisplayMessage("Unable to load input profile '" + base + "' for device '" +
                             controller->GetName() + "'",
                         display_message_ms);
  }
}

std::vector<std::string>
DeviceProfileManager::GetMatchingProfilesFromSetting(const std::string& setting,
                                                     const std::vector<std::string>& profiles)
{
  const std::string device_profile_root_location(File::GetUserPath(D_CONFIG_IDX) + "Profiles/" +
                                                 m_device_configuration->GetProfileName() + "/");

  const auto& profiles_from_setting = GetProfilesFromSetting(setting, device_profile_root_location);
  if (profiles_from_setting.empty())
  {
    return {};
  }

  std::vector<std::string> result;
  std::set_intersection(profiles.begin(), profiles.end(), profiles_from_setting.begin(),
                        profiles_from_setting.end(), std::back_inserter(result));
  return result;
}

bool DeviceProfileManager::UpdateProfile(int index)
{
  const auto& profiles = GetProfilesForDevice(m_device_configuration);
  if (profiles.empty())
  {
    Core::DisplayMessage("No input profiles found", display_message_ms);
    return false;
  }
  const std::string profile = GetProfile(index, profiles);

  auto* controller = m_device_configuration->GetController(m_controller_index);
  if (controller)
  {
    UpdateToProfile(profile, controller);
  }
  else
  {
    Core::DisplayMessage("No controller found for index: " + std::to_string(m_controller_index),
                         display_message_ms);
    return false;
  }

  return true;
}

bool DeviceProfileManager::UpdateProfileForGame(int index, const std::string& setting)
{
  const auto& profiles = GetProfilesForDevice(m_device_configuration);
  if (profiles.empty())
  {
    Core::DisplayMessage("No input profiles found", display_message_ms);
    return false;
  }

  if (setting.empty())
  {
    Core::DisplayMessage("No setting found for game", display_message_ms);
    return false;
  }

  const auto& profiles_for_game = GetMatchingProfilesFromSetting(setting, profiles);
  if (profiles_for_game.empty())
  {
    Core::DisplayMessage("No input profiles found for game", display_message_ms);
    return false;
  }

  const std::string profile = GetProfile(index, profiles_for_game);

  auto* controller = m_device_configuration->GetController(m_controller_index);
  if (controller)
  {
    UpdateToProfile(profile, controller);
  }
  else
  {
    Core::DisplayMessage("No controller found for index: " + std::to_string(m_controller_index),
                         display_message_ms);
    return false;
  }

  return true;
}

std::string DeviceProfileManager::GetInputProfilesForGame()
{
  IniFile game_ini = SConfig::GetInstance().LoadGameIni();
  const IniFile::Section* const control_section = game_ini.GetOrCreateSection("Controls");

  std::string result;
  control_section->Get(
      StringFromFormat("%s%d", m_controller_setting_name.c_str(), m_controller_index + 1), &result);
  return result;
}

void DeviceProfileManager::NextProfile()
{
  if (UpdateProfile(m_profile_index + 1))
  {
    m_profile_index++;
    m_modified = false;
    m_default = false;
  }
}

void DeviceProfileManager::PreviousProfile()
{
  if (UpdateProfile(m_profile_index - 1))
  {
    m_profile_index--;
    m_modified = false;
    m_default = false;
  }
}

void DeviceProfileManager::NextProfileForGame()
{
  UpdateProfileForGame(0, GetInputProfilesForGame());
  m_modified = false;
  m_default = false;
}

void DeviceProfileManager::PreviousProfileForGame()
{
  UpdateProfileForGame(0, GetInputProfilesForGame());
  m_modified = false;
  m_default = false;
}

bool DeviceProfileManager::SetProfile(int profile_index)
{
  if (UpdateProfile(profile_index))
  {
    m_profile_index = profile_index;
    m_modified = false;
    m_default = false;
    return true;
  }

  return false;
}

bool DeviceProfileManager::SetGameProfile(int profile_index)
{
  if (UpdateProfileForGame(profile_index, GetInputProfilesForGame()))
  {
    m_profile_index = profile_index;
    m_modified = false;
    m_default = false;
    return true;
  }

  return false;
}

void DeviceProfileManager::SetDefault()
{
  m_default = true;
}

void DeviceProfileManager::SetModified()
{
  m_modified = true;
}

std::string DeviceProfileManager::GetProfileName() const
{
  if (m_default)
  {
    if (m_modified)
    {
      return "DEFAULT*";
    }
    else
    {
      return "DEFAULT";
    }
  }

  std::string basename;
  SplitPath(m_profile_path, nullptr, &basename, nullptr);

  return basename + (m_modified ? "*" : "");
}

ProfileManager::ProfileManager()
    : m_wii_devices{DeviceProfileManager{Wiimote::GetConfig(), 0, "WiimoteProfile"},
                    DeviceProfileManager{Wiimote::GetConfig(), 1, "WiimoteProfile"},
                    DeviceProfileManager{Wiimote::GetConfig(), 2, "WiimoteProfile"},
                    DeviceProfileManager{Wiimote::GetConfig(), 3, "WiimoteProfile"}},
      m_gc_devices{DeviceProfileManager{Pad::GetConfig(), 0, "PadProfile"},
                   DeviceProfileManager{Pad::GetConfig(), 1, "PadProfile"},
                   DeviceProfileManager{Pad::GetConfig(), 2, "PadProfile"},
                   DeviceProfileManager{Pad::GetConfig(), 3, "PadProfile"}}
{
}

DeviceProfileManager* ProfileManager::GetGCDeviceProfileManager(int controller_index)
{
  return &m_gc_devices[controller_index];
}

DeviceProfileManager* ProfileManager::GetWiiDeviceProfileManager(int controller_index)
{
  return &m_wii_devices[controller_index];
}

}  // namespace InputProfile
