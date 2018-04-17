// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"

#include "Core/Config/WiimoteInputSettings.h"
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

  std::vector<std::string> ProfileCycler::GetProfilesForDevice(InputConfig* device_configuration)
  {
    const std::string device_profile_root_location(File::GetUserPath(D_CONFIG_IDX) + "Profiles/" + device_configuration->GetProfileName());
    return Common::DoFileSearch({ device_profile_root_location }, { ".ini" });
  }

  std::string ProfileCycler::GetProfile(CycleDirection cycle_direction, int& profile_index, const std::vector<std::string>& profiles)
  {
    // update the index and bind it to the number of available strings
    auto positive_modulo = [](int& i, int n) {i = (i % n + n) % n;};
    profile_index += static_cast<int>(cycle_direction);
    positive_modulo(profile_index, static_cast<int>(profiles.size()));

    return profiles[profile_index];
  }

  void ProfileCycler::UpdateToProfile(const std::string& profile_filename, const std::vector<ControllerEmu::EmulatedController*>& controllers)
  {
    std::string base;
    SplitPath(profile_filename, nullptr, &base, nullptr);

    IniFile ini_file;
    if (ini_file.Load(profile_filename))
    {
      Core::DisplayMessage("Loading input profile: " + base, display_message_ms);

      for (auto* controller : controllers)
      {
        controller->LoadConfig(ini_file.GetOrCreateSection("Profile"));
        controller->UpdateReferences(g_controller_interface);
      }
    }
    else
    {
      Core::DisplayMessage("Unable to load input profile: " + base, display_message_ms);
    }
  }

  std::vector<ControllerEmu::EmulatedController*> ProfileCycler::GetControllersForDevice(InputConfig* device_configuration)
  {
    const std::size_t size = device_configuration->GetControllerCount();

    std::vector<ControllerEmu::EmulatedController*> result(size);

    for (int i = 0; i < static_cast<int>(size); i++)
    {
      result[i] = device_configuration->GetController(i);
    }

    return result;
  }

  std::vector<std::string> ProfileCycler::GetProfilesFromSetting(const std::string& setting, InputConfig* device_configuration)
  {
    const auto& profiles = SplitString(setting, ',');

    const std::string device_profile_root_location(File::GetUserPath(D_CONFIG_IDX) + "Profiles/" + device_configuration->GetProfileName());

    std::vector<std::string> result(profiles.size());
    std::transform(profiles.begin(), profiles.end(), result.begin(), [&device_profile_root_location](const std::string& profile)
    {
      return device_profile_root_location + "/" + profile;
    });

    return result;
  }

  std::vector<std::string> ProfileCycler::GetMatchingProfilesFromSetting(const std::string& setting, const std::vector<std::string>& profiles, InputConfig* device_configuration)
  {
    const auto& profiles_from_setting = GetProfilesFromSetting(setting, device_configuration);
    if (profiles_from_setting.empty())
    {
      return {};
    }

    std::vector<std::string> result;
    std::set_intersection(profiles.begin(), profiles.end(), profiles_from_setting.begin(),
                          profiles_from_setting.end(), std::back_inserter(result));
    return result;
  }

  void ProfileCycler::CycleProfile(CycleDirection cycle_direction,
                                   InputConfig* device_configuration, int& profile_index)
  {
    const auto& profiles = GetProfilesForDevice(device_configuration);
    if (profiles.empty())
    {
      Core::DisplayMessage("No input profiles found", display_message_ms);
      return;
    }
    const std::string profile = GetProfile(cycle_direction, profile_index, profiles);

    const auto& controllers = GetControllersForDevice(device_configuration);
    UpdateToProfile(profile, controllers);
  }

  void ProfileCycler::CycleProfileForGame(CycleDirection cycle_direction,
                                          InputConfig* device_configuration, int& profile_index,
                                          const std::string& setting)
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

    const auto& profiles_for_game = GetMatchingProfilesFromSetting(setting, profiles,
                                                                   device_configuration);
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
      Core::DisplayMessage("No controller found for index: " + std::to_string(controller_index), display_message_ms);
    }
  }

  void ProfileCycler::NextWiimoteProfile()
  {
    CycleProfile(CycleDirection::Forward, Wiimote::GetConfig(), m_wiimote_profile_index);
  }

  void ProfileCycler::PreviousWiimoteProfile()
  {
    CycleProfile(CycleDirection::Backward, Wiimote::GetConfig(), m_wiimote_profile_index);
  }

  void ProfileCycler::NextWiimoteProfileForGame()
  {
    CycleProfileForGame(CycleDirection::Forward, Wiimote::GetConfig(), m_wiimote_profile_index,
                        Config::Get(Config::WIIMOTE_PROFILES));
  }

  void ProfileCycler::PreviousWiimoteProfileForGame()
  {
    CycleProfileForGame(CycleDirection::Backward, Wiimote::GetConfig(), m_wiimote_profile_index,
                        Config::Get(Config::WIIMOTE_PROFILES));
  }
}
