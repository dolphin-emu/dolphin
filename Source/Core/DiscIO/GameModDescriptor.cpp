// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/GameModDescriptor.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "Common/IOFile.h"
#include "Common/MathUtil.h"
#include "Common/StringUtil.h"

namespace DiscIO
{
static std::string MakeAbsolute(const std::string& directory, const std::string& path)
{
  return PathToString(StringToPath(directory) / StringToPath(path));
}

std::optional<GameModDescriptor> ParseGameModDescriptorFile(const std::string& filename)
{
  File::IOFile f(filename, "rb");
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
ParseRiivolutionOptions(const nlohmann::json& array)
{
  std::vector<GameModDescriptorRiivolutionPatchOption> options;
  for (const auto& option_object : array)
  {
    if (!option_object.is_object())
      continue;

    auto& option = options.emplace_back();
    for (const auto& [key, value] : option_object.items())
    {
      if (key == "section-name" && value.is_string())
        option.section_name = value.get<std::string>();
      else if (key == "option-id" && value.is_string())
        option.option_id = value.get<std::string>();
      else if (key == "option-name" && value.is_string())
        option.option_name = value.get<std::string>();
      else if (key == "choice" && value.is_number())
        option.choice = MathUtil::SaturatingCast<u32>(value.get<double>());
    }
  }
  return options;
}

static GameModDescriptorRiivolution ParseRiivolutionObject(const std::string& json_directory,
                                                           const nlohmann::json& object)
{
  GameModDescriptorRiivolution r;
  for (const auto& [element_key, element_value] : object.items())
  {
    if (element_key != "patches" || !element_value.is_array())
      continue;

    for (const auto& patch_object : element_value)
    {
      if (!patch_object.is_object())
        continue;

      auto& patch = r.patches.emplace_back();
      for (const auto& [key, value] : patch_object.items())
      {
        if (key == "xml" && value.is_string())
          patch.xml = MakeAbsolute(json_directory, value.get<std::string>());
        else if (key == "root" && value.is_string())
          patch.root = MakeAbsolute(json_directory, value.get<std::string>());
        else if (key == "options" && value.is_array())
          patch.options = ParseRiivolutionOptions(value);
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

  nlohmann::json json_root = nlohmann::json::parse(json, nullptr, false);
  if (json_root.is_discarded())
    return std::nullopt;
  if (!json_root.is_object())
    return std::nullopt;

  GameModDescriptor descriptor;
  bool is_game_mod_descriptor = false;
  bool is_valid_version = false;
  for (const auto& [key, value] : json_root.items())
  {
    if (key == "type" && value.is_string())
    {
      is_game_mod_descriptor = value.get<std::string>() == "dolphin-game-mod-descriptor";
    }
    else if (key == "version" && value.is_number())
    {
      is_valid_version = value.get<double>() == 1.0;
    }
    else if (key == "base-file" && value.is_string())
    {
      descriptor.base_file = MakeAbsolute(json_directory, value.get<std::string>());
    }
    else if (key == "display-name" && value.is_string())
    {
      descriptor.display_name = value.get<std::string>();
    }
    else if (key == "maker" && value.is_string())
    {
      descriptor.maker = value.get<std::string>();
    }
    else if (key == "banner" && value.is_string())
    {
      descriptor.banner = MakeAbsolute(json_directory, value.get<std::string>());
    }
    else if (key == "riivolution" && value.is_object())
    {
      descriptor.riivolution = ParseRiivolutionObject(json_directory, value);
    }
  }
  if (!is_game_mod_descriptor || !is_valid_version)
    return std::nullopt;
  return descriptor;
}

static nlohmann::json
WriteGameModDescriptorRiivolution(const GameModDescriptorRiivolution& riivolution)
{
  nlohmann::json json_patches;
  for (const auto& patch : riivolution.patches)
  {
    nlohmann::json json_patch;
    if (!patch.xml.empty())
      json_patch["xml"] = patch.xml;
    if (!patch.root.empty())
      json_patch["root"] = patch.root;
    if (!patch.options.empty())
    {
      nlohmann::json json_options;
      for (const auto& option : patch.options)
      {
        nlohmann::json json_option;
        if (!option.section_name.empty())
          json_option["section-name"] = option.section_name;
        if (!option.option_id.empty())
          json_option["option-id"] = option.option_id;
        if (!option.option_name.empty())
          json_option["option-name"] = option.option_name;
        json_option["choice"] = option.choice;
        json_options.push_back(std::move(json_option));
      }
      json_patch["options"] = std::move(json_options);
    }
    json_patches.push_back(std::move(json_patch));
  }

  nlohmann::json json_riivolution;
  json_riivolution["patches"] = std::move(json_patches);
  return json_riivolution;
}

std::string WriteGameModDescriptorString(const GameModDescriptor& descriptor, bool pretty)
{
  nlohmann::json json_root;
  json_root["type"] = "dolphin-game-mod-descriptor";
  json_root["version"] = 1.0;
  if (!descriptor.base_file.empty())
    json_root["base-file"] = descriptor.base_file;
  if (!descriptor.display_name.empty())
    json_root["display-name"] = descriptor.display_name;
  if (!descriptor.maker.empty())
    json_root["maker"] = descriptor.maker;
  if (!descriptor.banner.empty())
    json_root["banner"] = descriptor.banner;
  if (descriptor.riivolution)
    json_root["riivolution"] = WriteGameModDescriptorRiivolution(*descriptor.riivolution);
  return json_root.dump(pretty ? 2 : 0);
}

bool WriteGameModDescriptorFile(const std::string& filename, const GameModDescriptor& descriptor,
                                bool pretty)
{
  auto json = WriteGameModDescriptorString(descriptor, pretty);
  if (json.empty())
    return false;

  File::IOFile f(filename, "wb");
  if (!f)
    return false;

  if (!f.WriteString(json))
    return false;

  return true;
}
}  // namespace DiscIO
