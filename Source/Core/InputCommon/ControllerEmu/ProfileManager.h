// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <set>

#include "InputCommon/DeviceProfileContainer.h"
#include "InputCommon/InputProfile.h"

namespace InputCommon
{
class ProfileManager
{
public:
  explicit ProfileManager(ControllerEmu::EmulatedController* owning_controller);
  ~ProfileManager();
  void NextProfile();
  void PreviousProfile();
  void NextProfileForGame();
  void PreviousProfileForGame();

  void SetProfile(int profile_index);
  void SetProfile(const std::string& path);
  void SetProfileForGame(int profile_index);
  const std::optional<InputProfile>& GetProfile() const;
  void ClearProfile();

  void SetDeviceProfileContainer(DeviceProfileContainer* container);

  int GetProfileIndex();

  void AddToProfileFilter(const std::string& path);
  void ClearProfileFilter();
  std::size_t ProfilesInFilter() const;

private:
  ControllerEmu::EmulatedController* m_controller;
  int m_profile_index = -1;
  std::optional<InputProfile> m_profile;
  DeviceProfileContainer* m_container;
  std::set<std::string> m_profile_search_filter;
};
}  // namespace InputCommon
