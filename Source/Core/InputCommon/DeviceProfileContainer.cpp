// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/DeviceProfileContainer.h"

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"

namespace InputCommon
{
namespace
{
int ClampContainer(int index, std::size_t container_size)
{
  auto positive_modulo = [](int& i, int n) { i = (i % n + n) % n; };
  positive_modulo(index, static_cast<int>(container_size));
  return index;
}
}  // namespace

void DeviceProfileContainer::Load(const std::string& profile_type)
{
  m_profiles.clear();
  m_path_to_profile.clear();
  const std::string device_profile_root_location(File::GetUserPath(D_CONFIG_IDX) + "Profiles/" +
                                                 profile_type);
  const auto profiles = Common::DoFileSearch({device_profile_root_location}, {".ini"}, true);
  for (auto profile : profiles)
  {
    EnsureProfile(profile, device_profile_root_location);
  }
}

const std::vector<InputProfile>& DeviceProfileContainer::GetAllProfiles() const
{
  return m_profiles;
}

std::optional<InputProfile> DeviceProfileContainer::GetProfile(int& index)
{
  if (m_profiles.empty())
    return {};

  index = ClampContainer(index, m_profiles.size());
  return m_profiles[index];
}

std::optional<InputProfile> DeviceProfileContainer::GetProfile(int& index, Direction direction,
                                                               std::set<std::string> filter_list)
{
  if (m_profiles.empty())
    return {};

  if (direction == Direction::Forward)
  {
    for (int i = index; i < static_cast<int>(m_profiles.size()); i++)
    {
      const auto profile = m_profiles[i];
      if (filter_list.count(profile.GetAbsolutePath()))
      {
        index = i;
        return m_profiles[index];
      }
    }

    index = 0;
  }
  else
  {
    for (int i = index; i > 0; i--)
    {
      const auto profile = m_profiles[i];
      if (filter_list.count(profile.GetAbsolutePath()))
      {
        index = i;
        return m_profiles[index];
      }
    }

    index = static_cast<int>(m_profiles.size()) - 1;
  }

  return GetProfile(index, direction, std::move(filter_list));
}

int DeviceProfileContainer::GetIndexFromPath(const std::string& profile_path)
{
  auto iter = m_path_to_profile.find(profile_path);
  if (iter != m_path_to_profile.end())
  {
    return iter->second;
  }

  return -1;
}

void DeviceProfileContainer::EnsureProfile(const std::string& profile_path,
                                           const std::string& profiles_root)
{
  auto iter = m_path_to_profile.find(profile_path);
  if (iter != m_path_to_profile.end())
  {
    m_profiles[iter->second] = InputProfile{profile_path, profiles_root};
    return;
  }

  m_profiles.emplace_back(InputProfile{profile_path, profiles_root});
  m_path_to_profile.emplace(profile_path, static_cast<int>(m_profiles.size() - 1));
}

bool DeviceProfileContainer::DeleteProfile(const std::string& profile_path)
{
  auto iter = m_path_to_profile.find(profile_path);
  if (iter == m_path_to_profile.end())
  {
    return false;
  }

  m_profiles.erase(m_profiles.begin() + iter->second);
  m_path_to_profile.erase(iter);
  File::Delete(profile_path);
  return true;
}
}  // namespace InputCommon
