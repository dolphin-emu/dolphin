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
  private:
    void CycleProfile(CycleDirection cycle_direction, InputConfig* device_configuration, int& profile_index);
    std::vector<std::string> GetProfilesForDevice(InputConfig* device_configuration);
    std::string GetProfile(CycleDirection cycle_direction, int& profile_index, const std::vector<std::string>& profiles);
    void UpdateToProfile(const std::string& profile_filename, const std::vector<ControllerEmu::EmulatedController*>& controllers);
    std::vector<ControllerEmu::EmulatedController*> GetControllersForDevice(InputConfig* device_configuration);

    int m_wiimote_profile_index = 0;
  };
}
