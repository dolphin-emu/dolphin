// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

class InputConfig;

namespace ControllerEmu
{
  class EmulatedController;
}

#include <string>
#include <vector>

namespace InputProfile
{
  enum class CycleDirection : int
  {
    Forward = 1,
    Backward = -1
  };

  class ProfileCycler
  {
  public:
    void NextWiimoteProfile();
    void PreviousWiimoteProfile();
    void NextWiimoteProfileForGame();
    void PreviousWiimoteProfileForGame();
  private:
    void CycleProfile(CycleDirection cycle_direction, InputConfig* device_configuration, int& profile_index);
    void CycleProfileForGame(CycleDirection cycle_direction, InputConfig* device_configuration, int& profile_index, const std::string& setting);
    std::vector<std::string> GetProfilesForDevice(InputConfig* device_configuration);
    std::vector<std::string> GetProfilesFromSetting(const std::string& setting, InputConfig* device_configuration);
    std::string GetProfile(CycleDirection cycle_direction, int& profile_index, const std::vector<std::string>& profiles);
    std::vector<std::string> GetMatchingProfilesFromSetting(const std::string& setting, const std::vector<std::string>& profiles, InputConfig* device_configuration);
    void UpdateToProfile(const std::string& profile_filename, const std::vector<ControllerEmu::EmulatedController*>& controllers);
    std::vector<ControllerEmu::EmulatedController*> GetControllersForDevice(InputConfig* device_configuration);

    int m_wiimote_profile_index = 0;
  };
}
