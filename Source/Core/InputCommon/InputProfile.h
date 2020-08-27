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
std::vector<std::string> GetProfilesFromSetting(const std::string& setting,
                                                const std::string& root);

enum class CycleDirection : int
{
  Forward = 1,
  Backward = -1
};

class ProfileCycler
{
public:
  void NextWiimoteProfile(int controller_index);
  void PreviousWiimoteProfile(int controller_index);
  void NextWiimoteProfileForGame(int controller_index);
  void PreviousWiimoteProfileForGame(int controller_index);

private:
  void CycleProfile(CycleDirection cycle_direction, InputConfig* device_configuration,
                    int& profile_index, int controller_index);
  void CycleProfileForGame(CycleDirection cycle_direction, InputConfig* device_configuration,
                           int& profile_index, const std::string& setting, int controller_index);
  std::vector<std::string> GetProfilesForDevice(InputConfig* device_configuration);
  std::string GetProfile(CycleDirection cycle_direction, int& profile_index,
                         const std::vector<std::string>& profiles);
  std::vector<std::string> GetMatchingProfilesFromSetting(const std::string& setting,
                                                          const std::vector<std::string>& profiles,
                                                          InputConfig* device_configuration);
  void UpdateToProfile(const std::string& profile_filename,
                       ControllerEmu::EmulatedController* controller);
  std::string GetWiimoteInputProfilesForGame(int controller_index);

  int m_wiimote_profile_index = 0;
};
}  // namespace InputProfile
