// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "DiscIO/GameModDescriptor.h"

#include <picojson.h>

#include "Common/IOFile.h"
#include "Common/MathUtil.h"
#include "Common/StringUtil.h"

namespace DiscIO
{
static std::string MakeAbsolute(const std::string& directory, const std::string& path)
{
#ifdef _WIN32
  return PathToString(StringToPath(directory) / StringToPath(path));
#else
  if (StringBeginsWith(path, "/"))
    return path;
  return directory + "/" + path;
#endif
}

std::optional<GameModDescriptor> ParseGameModDescriptorFile(const std::string& filename)
{
  ::File::IOFile f(filename, "rb");
  if (!f)
    return std::nullopt;

  std::vector<char> data;
  data.resize(f.GetSize());
  if (!f.ReadBytes(data.data(), data.size()))
    return std::nullopt;

#ifdef _WIN32
  std::string path = ReplaceAll(filename, "\\", "/");
#else
  const std::string& path = filename;
#endif
  return ParseGameModDescriptorString(std::string_view(data.data(), data.size()), path);
}

static std::vector<GameModDescriptorRiivolutionPatchOption>
ParseRiivolutionOptions(const picojson::array& array)
{
  std::vector<GameModDescriptorRiivolutionPatchOption> options;
  for (const auto& option_object : array)
  {
    if (!option_object.is<picojson::object>())
      continue;

    auto& option = options.emplace_back();
    for (const auto& [key, value] : option_object.get<picojson::object>())
    {
      if (key == "section-name" && value.is<std::string>())
        option.section_name = value.get<std::string>();
      else if (key == "option-id" && value.is<std::string>())
        option.option_id = value.get<std::string>();
      else if (key == "option-name" && value.is<std::string>())
        option.option_name = value.get<std::string>();
      else if (key == "choice" && value.is<double>())
        option.choice = MathUtil::SaturatingCast<u32>(value.get<double>());
    }
  }
  return options;
}

static GameModDescriptorRiivolution ParseRiivolutionObject(const std::string& json_directory,
                                                           const picojson::object& object)
{
  GameModDescriptorRiivolution r;
  for (const auto& [element_key, element_value] : object)
  {
    if (element_key == "patches" && element_value.is<picojson::array>())
    {
      for (const auto& patch_object : element_value.get<picojson::array>())
      {
        if (!patch_object.is<picojson::object>())
          continue;

        auto& patch = r.patches.emplace_back();
        for (const auto& [key, value] : patch_object.get<picojson::object>())
        {
          if (key == "xml" && value.is<std::string>())
            patch.xml = MakeAbsolute(json_directory, value.get<std::string>());
          else if (key == "root" && value.is<std::string>())
            patch.root = MakeAbsolute(json_directory, value.get<std::string>());
          else if (key == "options" && value.is<picojson::array>())
            patch.options = ParseRiivolutionOptions(value.get<picojson::array>());
        }
      }
    }
  }
  return r;
}

std::optional<GameModDescriptor> ParseGameModDescriptorString(std::string_view json,
                                                              std::string_view json_path)
{
  std::string json_directory;
  SplitPath(json_path, &json_directory, nullptr, nullptr);

  picojson::value json_root;
  std::string err;
  picojson::parse(json_root, json.begin(), json.end(), &err);
  if (!err.empty())
    return std::nullopt;
  if (!json_root.is<picojson::object>())
    return std::nullopt;

  GameModDescriptor descriptor;
  bool is_valid_version = false;
  for (const auto& [key, value] : json_root.get<picojson::object>())
  {
    if (key == "version" && value.is<double>())
    {
      is_valid_version = value.get<double>() == 1.0;
    }
    else if (key == "base-file" && value.is<std::string>())
    {
      descriptor.base_file = MakeAbsolute(json_directory, value.get<std::string>());
    }
    else if (key == "display-name" && value.is<std::string>())
    {
      descriptor.display_name = value.get<std::string>();
    }
    else if (key == "banner" && value.is<std::string>())
    {
      descriptor.banner = MakeAbsolute(json_directory, value.get<std::string>());
    }
    else if (key == "riivolution" && value.is<picojson::object>())
    {
      descriptor.riivolution =
          ParseRiivolutionObject(json_directory, value.get<picojson::object>());
    }
  }
  if (!is_valid_version)
    return std::nullopt;
  return descriptor;
}
}  // namespace DiscIO
