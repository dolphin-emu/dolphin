// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/JsonUtil.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "Common/FileUtil.h"

nlohmann::json ToJsonObject(const Common::Vec3& vec)
{
  return {{"x", vec.x}, {"y", vec.y}, {"z", vec.z}};
}

void FromJson(const nlohmann::json& obj, Common::Vec3& vec)
{
  vec.x = obj.value("x", 0.0f);
  vec.y = obj.value("y", 0.0f);
  vec.z = obj.value("z", 0.0f);
}

std::optional<std::string> ReadStringFromJson(const nlohmann::json& obj, const std::string_view key)
{
  auto it = obj.find(key);
  if (it == obj.end() || !it->is_string())
    return std::nullopt;

  return it->get<std::string>();
}

std::optional<bool> ReadBoolFromJson(const nlohmann::json& obj, const std::string_view key)
{
  auto it = obj.find(key);
  if (it == obj.end() || !it->is_boolean())
    return std::nullopt;

  return it->get<bool>();
}

std::optional<nlohmann::json> ReadObjectFromJson(const nlohmann::json& obj,
                                                 const std::string_view key)
{
  auto it = obj.find(key);
  if (it == obj.end() || !it->is_object())
    return std::nullopt;

  return *it;
}

std::optional<nlohmann::json> ReadArrayFromJson(const nlohmann::json& obj,
                                                const std::string_view key)
{
  auto it = obj.find(key);
  if (it == obj.end() || !it->is_array())
    return std::nullopt;

  return *it;
}

bool JsonToFile(const std::string& filename, const nlohmann::json& root, bool prettify)
{
  std::ofstream json_stream;
  File::OpenFStream(json_stream, filename, std::ios_base::out);
  if (!json_stream.is_open())
    return false;

  json_stream << root.dump(prettify ? 2 : -1);

  return true;
}

bool JsonFromFile(const std::string& filename, nlohmann::json* root, std::string* error)
{
  std::string json_data;
  if (!File::ReadFileToString(filename, json_data))
  {
    if (error)
      *error = "Failed to read file";
    return false;
  }

  nlohmann::json parsed_json;
  auto result = nlohmann::json::parse(json_data, nullptr, false);
  if (result.is_discarded())
  {
    if (error)
      *error = "Failed to parse JSON (invalid format)";
    return false;
  }

  *root = std::move(result);
  return true;
}
