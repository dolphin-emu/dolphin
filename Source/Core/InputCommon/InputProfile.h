// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

class InputConfig;

namespace ControllerEmu
{
class EmulatedController;
}

#include <array>
#include <string>
#include <vector>

namespace InputProfile
{
std::vector<std::string> GetProfilesFromSetting(const std::string& setting,
                                                const std::string& root);

class DeviceProfileManager
{
public:
  DeviceProfileManager(InputConfig* device_configuration, int controller_index,
                       std::string setting_name);

  static std::vector<std::string> GetProfilesForDevice(InputConfig* device_configuration);

  void NextProfile();
  void PreviousProfile();
  void NextProfileForGame();
  void PreviousProfileForGame();

  bool SetProfile(int profile_index);
  bool SetGameProfile(int profile_index);

  void SetDefault();
  void SetModified();

  std::string GetProfileName() const;

private:
  bool UpdateProfile(int index);
  bool UpdateProfileForGame(int index, const std::string& setting);
  std::string GetProfile(int index, const std::vector<std::string>& profiles);
  std::vector<std::string> GetMatchingProfilesFromSetting(const std::string& setting,
                                                          const std::vector<std::string>& profiles);
  void UpdateToProfile(const std::string& profile_filename,
                       ControllerEmu::EmulatedController* controller);
  std::string GetInputProfilesForGame();

  bool m_modified = false;
  bool m_default = true;
  int m_profile_index = 0;
  int m_controller_index = 0;
  std::string m_profile_path;
  std::string m_controller_setting_name;
  InputConfig* m_device_configuration;
};

class ProfileManager
{
public:
  ProfileManager();
  DeviceProfileManager* GetGCDeviceProfileManager(int controller_index);
  DeviceProfileManager* GetWiiDeviceProfileManager(int controller_index);

private:
  std::array<DeviceProfileManager, 4> m_wii_devices;
  std::array<DeviceProfileManager, 4> m_gc_devices;
};
}  // namespace InputProfile

extern InputProfile::ProfileManager g_profile_manager;
