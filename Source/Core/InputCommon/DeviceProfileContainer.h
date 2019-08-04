// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputProfile.h"

#include <map>
#include <optional>
#include <set>
#include <vector>

namespace InputCommon
{
class DeviceProfileContainer
{
public:
  enum class Direction
  {
    Forward,
    Backward
  };
  void Load(const std::string& profile_type);
  const std::vector<InputProfile>& GetAllProfiles() const;
  std::optional<InputProfile> GetProfile(int& index);
  std::optional<InputProfile> GetProfile(int& index, Direction direction,
                                         std::set<std::string> filter_list);
  int GetIndexFromPath(const std::string& profile_path);

  void EnsureProfile(const std::string& profile_path, const std::string& profiles_root);
  bool DeleteProfile(const std::string& profile_path);

private:
  std::vector<InputProfile> m_profiles;
  std::map<std::string, int> m_path_to_profile;
};
}  // namespace InputCommon
