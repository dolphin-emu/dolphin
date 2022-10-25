// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <sstream>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

namespace Platform
{
struct BuildVersion
{
  u32 major{};
  u32 minor{};
  u32 build{};
  auto operator<=>(BuildVersion const& rhs) const = default;
  static std::optional<BuildVersion> from_string(const std::string& str)
  {
    auto components = SplitString(str, '.');
    // Allow variable number of components (truncating after "build"), but not
    // empty.
    if (components.size() == 0)
      return {};
    BuildVersion version;
    if (!TryParse(components[0], &version.major, 10))
      return {};
    if (components.size() > 1 && !TryParse(components[1], &version.minor, 10))
      return {};
    if (components.size() > 2 && !TryParse(components[2], &version.build, 10))
      return {};
    return version;
  }
};

enum class VersionCheckStatus
{
  NothingToDo,
  UpdateOptional,
  UpdateRequired,
};

struct VersionCheckResult
{
  VersionCheckStatus status{VersionCheckStatus::NothingToDo};
  std::optional<BuildVersion> current_version{};
  std::optional<BuildVersion> target_version{};
};

class BuildInfo
{
  using Map = std::map<std::string, std::string>;

public:
  BuildInfo() = default;
  BuildInfo(const std::string& content);

  std::optional<std::string> GetString(const std::string& name) const
  {
    auto it = map.find(name);
    if (it == map.end() || it->second.size() == 0)
      return {};
    return it->second;
  }

  std::optional<BuildVersion> GetVersion(const std::string& name) const
  {
    auto str = GetString(name);
    if (!str.has_value())
      return {};
    return BuildVersion::from_string(str.value());
  }

private:
  void Parse(const std::string& content)
  {
    std::stringstream content_stream(content);
    std::string line;
    while (std::getline(content_stream, line))
    {
      if (line.starts_with("//"))
        continue;
      const size_t equals_index = line.find('=');
      if (equals_index == line.npos)
        continue;
      auto key = line.substr(0, equals_index);
      auto key_it = map.find(key);
      if (key_it == map.end())
        continue;
      key_it->second = line.substr(equals_index + 1);
    }
  }
  Map map;
};

bool VersionCheck(const BuildInfo& this_build_info, const BuildInfo& next_build_info);
}  // namespace Platform
