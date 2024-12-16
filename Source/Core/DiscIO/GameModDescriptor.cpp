// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/GameModDescriptor.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <picojson.h>

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
    else if (element_key == "force-console-ng-id" && element_value.is<double>())
    {
      // JSON contains hardcoded console ID, use that instead of the one from the NAND
      r.console_ng_id = MathUtil::SaturatingCast<u32>(element_value.get<double>());
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
  bool is_game_mod_descriptor = false;
  bool is_valid_version = false;
  for (const auto& [key, value] : json_root.get<picojson::object>())
  {
    if (key == "type" && value.is<std::string>())
    {
      is_game_mod_descriptor = value.get<std::string>() == "dolphin-game-mod-descriptor";
    }
    else if (key == "version" && value.is<double>())
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
    else if (key == "maker" && value.is<std::string>())
    {
      descriptor.maker = value.get<std::string>();
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
  if (!is_game_mod_descriptor || !is_valid_version)
    return std::nullopt;
  return descriptor;
}

static picojson::object
WriteGameModDescriptorRiivolution(const GameModDescriptorRiivolution& riivolution)
{
  picojson::array json_patches;
  for (const auto& patch : riivolution.patches)
  {
    picojson::object json_patch;
    if (!patch.xml.empty())
      json_patch["xml"] = picojson::value(patch.xml);
    if (!patch.root.empty())
      json_patch["root"] = picojson::value(patch.root);
    if (!patch.options.empty())
    {
      picojson::array json_options;
      for (const auto& option : patch.options)
      {
        picojson::object json_option;
        if (!option.section_name.empty())
          json_option["section-name"] = picojson::value(option.section_name);
        if (!option.option_id.empty())
          json_option["option-id"] = picojson::value(option.option_id);
        if (!option.option_name.empty())
          json_option["option-name"] = picojson::value(option.option_name);
        json_option["choice"] = picojson::value(static_cast<double>(option.choice));
        json_options.emplace_back(std::move(json_option));
      }
      json_patch["options"] = picojson::value(std::move(json_options));
    }
    json_patches.emplace_back(std::move(json_patch));
  }

  picojson::object json_riivolution;
  json_riivolution["patches"] = picojson::value(std::move(json_patches));
  return json_riivolution;
}

std::string WriteGameModDescriptorString(const GameModDescriptor& descriptor, bool pretty)
{
  picojson::object json_root;
  json_root["type"] = picojson::value("dolphin-game-mod-descriptor");
  json_root["version"] = picojson::value(1.0);
  if (!descriptor.base_file.empty())
    json_root["base-file"] = picojson::value(descriptor.base_file);
  if (!descriptor.display_name.empty())
    json_root["display-name"] = picojson::value(descriptor.display_name);
  if (!descriptor.maker.empty())
    json_root["maker"] = picojson::value(descriptor.maker);
  if (!descriptor.banner.empty())
    json_root["banner"] = picojson::value(descriptor.banner);
  if (descriptor.riivolution)
  {
    json_root["riivolution"] =
        picojson::value(WriteGameModDescriptorRiivolution(*descriptor.riivolution));
  }
  return picojson::value(json_root).serialize(pretty);
}

bool WriteGameModDescriptorFile(const std::string& filename, const GameModDescriptor& descriptor,
                                bool pretty)
{
  auto json = WriteGameModDescriptorString(descriptor, pretty);
  if (json.empty())
    return false;

  ::File::IOFile f(filename, "wb");
  if (!f)
    return false;

  if (!f.WriteString(json))
    return false;

  return true;
}
}  // namespace DiscIO
