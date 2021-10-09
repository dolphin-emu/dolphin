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
  if (std::filesystem::path(path).is_absolute())
    return path;
#else
  if (StringBeginsWith(path, "/"))
    return path;
#endif
  return directory + "/" + path;
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
    for (const auto& option_element : option_object.get<picojson::object>())
    {
      if (option_element.second.is<std::string>() &&
          option_element.first == std::string_view("section-name"))
      {
        option.m_section_name = option_element.second.get<std::string>();
      }
      else if (option_element.second.is<std::string>() &&
               option_element.first == std::string_view("option-id"))
      {
        option.m_option_id = option_element.second.get<std::string>();
      }
      else if (option_element.second.is<std::string>() &&
               option_element.first == std::string_view("option-name"))
      {
        option.m_option_name = option_element.second.get<std::string>();
      }
      else if (option_element.second.is<double>() &&
               option_element.first == std::string_view("choice"))
      {
        option.m_choice = MathUtil::SaturatingCast<u32>(option_element.second.get<double>());
      }
    }
  }
  return options;
}

static GameModDescriptorRiivolution ParseRiivolutionObject(const std::string& json_directory,
                                                           const picojson::object& object)
{
  GameModDescriptorRiivolution r;
  for (const auto& element : object)
  {
    if (element.second.is<picojson::array>() && element.first == std::string_view("patches"))
    {
      for (const auto& patch_object : element.second.get<picojson::array>())
      {
        if (!patch_object.is<picojson::object>())
          continue;

        auto& patch = r.m_patches.emplace_back();
        for (const auto& patch_element : patch_object.get<picojson::object>())
        {
          if (patch_element.second.is<std::string>() &&
              patch_element.first == std::string_view("xml"))
          {
            patch.m_xml = MakeAbsolute(json_directory, patch_element.second.get<std::string>());
          }
          else if (patch_element.second.is<std::string>() &&
                   patch_element.first == std::string_view("root"))
          {
            patch.m_root = MakeAbsolute(json_directory, patch_element.second.get<std::string>());
          }
          else if (patch_element.second.is<picojson::array>() &&
                   patch_element.first == std::string_view("options"))
          {
            patch.m_options = ParseRiivolutionOptions(patch_element.second.get<picojson::array>());
          }
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
  const auto& r = json_root.get<picojson::object>();

  GameModDescriptor g;
  bool is_valid_version = false;
  for (const auto& it : r)
  {
    if (it.second.is<double>() && it.first == std::string_view("version"))
    {
      is_valid_version = it.second.get<double>() == 1.0;
    }
    else if (it.second.is<std::string>() && it.first == std::string_view("base-file"))
    {
      g.m_base_file = MakeAbsolute(json_directory, it.second.get<std::string>());
    }
    else if (it.second.is<std::string>() && it.first == std::string_view("display-name"))
    {
      g.m_display_name = it.second.get<std::string>();
    }
    else if (it.second.is<std::string>() && it.first == std::string_view("banner"))
    {
      g.m_banner = MakeAbsolute(json_directory, it.second.get<std::string>());
    }
    else if (it.second.is<picojson::object>() && it.first == std::string_view("riivolution"))
    {
      g.m_riivolution = ParseRiivolutionObject(json_directory, it.second.get<picojson::object>());
    }
  }
  if (!is_valid_version)
    return std::nullopt;
  return g;
}
}  // namespace DiscIO
