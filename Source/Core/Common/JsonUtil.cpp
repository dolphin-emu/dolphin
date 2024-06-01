// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/JsonUtil.h"

#include <fstream>

#include "Common/FileUtil.h"

picojson::object ToJsonObject(const Common::Vec3& vec)
{
  picojson::object obj;
  obj.emplace("x", vec.x);
  obj.emplace("y", vec.y);
  obj.emplace("z", vec.z);
  return obj;
}

void FromJson(const picojson::object& obj, Common::Vec3& vec)
{
  vec.x = ReadNumericFromJson<float>(obj, "x").value_or(0.0f);
  vec.y = ReadNumericFromJson<float>(obj, "y").value_or(0.0f);
  vec.z = ReadNumericFromJson<float>(obj, "z").value_or(0.0f);
}

std::optional<std::string> ReadStringFromJson(const picojson::object& obj, const std::string& key)
{
  const auto it = obj.find(key);
  if (it == obj.end())
    return std::nullopt;
  if (!it->second.is<std::string>())
    return std::nullopt;
  return it->second.to_str();
}

std::optional<bool> ReadBoolFromJson(const picojson::object& obj, const std::string& key)
{
  const auto it = obj.find(key);
  if (it == obj.end())
    return std::nullopt;
  if (!it->second.is<bool>())
    return std::nullopt;
  return it->second.get<bool>();
}

bool JsonToFile(const std::string& filename, const picojson::value& root, bool prettify)
{
  std::ofstream json_stream;
  File::OpenFStream(json_stream, filename, std::ios_base::out);
  if (!json_stream.is_open())
  {
    return false;
  }
  json_stream << root.serialize(prettify);
  return true;
}

bool JsonFromFile(const std::string& filename, picojson::value* root, std::string* error)
{
  std::string json_data;
  if (!File::ReadFileToString(filename, json_data))
  {
    return false;
  }

  *error = picojson::parse(*root, json_data);
  return error->empty();
}
