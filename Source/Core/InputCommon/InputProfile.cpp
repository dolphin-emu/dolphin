// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HotkeyManager.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/InputProfile.h"

#include <algorithm>
#include <iterator>

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

std::vector<std::string> ProfileCycler::GetProfilesForDevice(InputConfig* device_configuration)
{
  const std::string device_profile_root_location(File::GetUserPath(D_CONFIG_IDX) + "Profiles/" +
                                                 device_configuration->GetProfileName());
  return Common::DoFileSearch({device_profile_root_location}, {".ini"}, true);
}

std::string ProfileCycler::GetProfile(CycleDirection cycle_direction, int& profile_index,
                                      const std::vector<std::string>& profiles)
{
  // update the index and bind it to the number of available strings
  auto positive_modulo = [](int& i, int n) { i = (i % n + n) % n; };
  profile_index += static_cast<int>(cycle_direction);
  positive_modulo(profile_index, static_cast<int>(profiles.size()));

  return profiles[profile_index];
}

void ProfileCycler::UpdateToProfile(const std::string& profile_filename,
                                    ControllerEmu::EmulatedController* controller)
{
  std::string base;
  SplitPath(profile_filename, nullptr, &base, nullptr);

  IniFile ini_file;
  if (ini_file.Load(profile_filename))
  {
    Core::DisplayMessage("Loading input profile '" + base + "' for device '" +
                             controller->GetName() + "'",
                         display_message_ms);
    controller->LoadConfig(ini_file.GetOrCreateSection("Profile"));
    controller->UpdateReferences(g_controller_interface);
  }
  else
  {
    Core::DisplayMessage("Unable to load input profile '" + base + "' for device '" +
                             controller->GetName() + "'",
                         display_message_ms);
  }
}

std::vector<std::string>
ProfileCycler::GetMatchingProfilesFromSetting(const std::string& setting,
                                              const std::vector<std::string>& profiles,
                                              InputConfig* device_configuration)
{
  const std::string device_profile_root_location(File::GetUserPath(D_CONFIG_IDX) + "Profiles/" +
                                                 device_configuration->GetProfileName() + "/");

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

void ProfileCycler::CycleProfile(CycleDirection cycle_direction, InputConfig* device_configuration,
                                 int& profile_index, int controller_index)
{
  const auto& profiles = GetProfilesForDevice(device_configuration);
  if (profiles.empty())
  {
    Core::DisplayMessage("No input profiles found", display_message_ms);
    return;
  }
  const std::string profile = GetProfile(cycle_direction, profile_index, profiles);

  auto* controller = device_configuration->GetController(controller_index);
  if (controller)
  {
    UpdateToProfile(profile, controller);
  }
  else
  {
    Core::DisplayMessage("No controller found for index: " + std::to_string(controller_index),
                         display_message_ms);
  }
}

void ProfileCycler::CycleProfileForGame(CycleDirection cycle_direction,
                                        InputConfig* device_configuration, int& profile_index,
                                        const std::string& setting, int controller_index)
{
  const auto& profiles = GetProfilesForDevice(device_configuration);
  if (profiles.empty())
  {
    Core::DisplayMessage("No input profiles found", display_message_ms);
    return;
  }

  if (setting.empty())
  {
    Core::DisplayMessage("No setting found for game", display_message_ms);
    return;
  }

  const auto& profiles_for_game =
      GetMatchingProfilesFromSetting(setting, profiles, device_configuration);
  if (profiles_for_game.empty())
  {
    Core::DisplayMessage("No input profiles found for game", display_message_ms);
    return;
  }

  const std::string profile = GetProfile(cycle_direction, profile_index, profiles_for_game);

  auto* controller = device_configuration->GetController(controller_index);
  if (controller)
  {
    UpdateToProfile(profile, controller);
  }
  else
  {
    Core::DisplayMessage("No controller found for index: " + std::to_string(controller_index),
                         display_message_ms);
  }
}

std::string ProfileCycler::GetWiimoteInputProfilesForGame(int controller_index)
{
  IniFile game_ini = SConfig::GetInstance().LoadGameIni();
  const IniFile::Section* const control_section = game_ini.GetOrCreateSection("Controls");

  std::string result;
  control_section->Get(StringFromFormat("WiimoteProfile%d", controller_index + 1), &result);
  return result;
}

void ProfileCycler::NextWiimoteProfile(int controller_index)
{
  CycleProfile(CycleDirection::Forward, Wiimote::GetConfig(), m_wiimote_profile_index,
               controller_index);
}

void ProfileCycler::PreviousWiimoteProfile(int controller_index)
{
  CycleProfile(CycleDirection::Backward, Wiimote::GetConfig(), m_wiimote_profile_index,
               controller_index);
}

void ProfileCycler::NextWiimoteProfileForGame(int controller_index)
{
  CycleProfileForGame(CycleDirection::Forward, Wiimote::GetConfig(), m_wiimote_profile_index,
                      GetWiimoteInputProfilesForGame(controller_index), controller_index);
}

void ProfileCycler::PreviousWiimoteProfileForGame(int controller_index)
{
  CycleProfileForGame(CycleDirection::Backward, Wiimote::GetConfig(), m_wiimote_profile_index,
                      GetWiimoteInputProfilesForGame(controller_index), controller_index);
}
}  // namespace InputProfile
