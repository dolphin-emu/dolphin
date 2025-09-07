// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/JsonUtil.h"

#include <fstream>

#include <nlohmann/detail/input/json_sax.hpp>
#include <nlohmann/json.hpp>

#include "Common/FileUtil.h"

class SaxJsonParser
    : public nlohmann::detail::json_sax_dom_parser<nlohmann::json,
                                                   nlohmann::detail::string_input_adapter_type>
{
public:
  explicit SaxJsonParser(nlohmann::json& j)
      : nlohmann::detail::json_sax_dom_parser<nlohmann::json,
                                              nlohmann::detail::string_input_adapter_type>(j, false)
  {
  }

  bool parse_error(std::size_t position, const std::string& last_token,
                   const nlohmann::json::exception& ex)
  {
    m_error = ex.what();
    return false;
  }

  std::string m_error;
};

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

  nlohmann::json result;
  SaxJsonParser json_parser(result);
  if (!nlohmann::json::sax_parse(json_data, &json_parser))
  {
    if (error)
      *error = json_parser.m_error;
    return false;
  }

  *root = std::move(result);
  return true;
}
