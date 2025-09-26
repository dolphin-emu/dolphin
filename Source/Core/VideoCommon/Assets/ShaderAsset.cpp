// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/ShaderAsset.h"

#include <algorithm>
#include <utility>

#include <nlohmann/json.hpp>

#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/ShaderGenCommon.h"

namespace VideoCommon
{
template <typename ElementType, std::size_t ElementCount, typename PropertyType>
bool ParseNumeric(const CustomAssetLibrary::AssetID& asset_id, const nlohmann::json& json_value,
                  std::string_view code_name, PropertyType* value)
{
  static_assert(ElementCount <= 4, "Numeric data expected to be four elements or less");
  if constexpr (ElementCount == 1)
  {
    if (!json_value.is_number())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' shader has attribute '{}' where "
                    "a double was expected but not provided.",
                    asset_id, code_name);
      return false;
    }

    *value = static_cast<ElementType>(json_value.get<double>());
  }
  else
  {
    if (!json_value.is_array())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' shader has attribute '{}' where "
                    "an array was expected but not provided.",
                    asset_id, code_name);
      return false;
    }

    if (json_value.size() != ElementCount)
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' shader has attribute '{}' with incorrect number "
                    "of elements, expected {}",
                    asset_id, code_name, ElementCount);
      return false;
    }

    if (!std::ranges::all_of(json_value, &nlohmann::json::is_number))
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' shader has attribute '{}' where "
                    "all elements are not of type double.",
                    asset_id, code_name);
      return false;
    }

    std::array<ElementType, ElementCount> data;
    for (std::size_t i = 0; i < ElementCount; i++)
    {
      data[i] = static_cast<ElementType>(json_value[i].get<double>());
    }
    *value = std::move(data);
  }

  return true;
}

static bool ParseShaderValue(const CustomAssetLibrary::AssetID& asset_id,
                             const nlohmann::json& json_value, std::string_view code_name,
                             std::string_view type, ShaderProperty::Value* value)
{
  if (type == "int")
  {
    return ParseNumeric<s32, 1>(asset_id, json_value, code_name, value);
  }
  else if (type == "int2")
  {
    return ParseNumeric<s32, 2>(asset_id, json_value, code_name, value);
  }
  else if (type == "int3")
  {
    return ParseNumeric<s32, 3>(asset_id, json_value, code_name, value);
  }
  else if (type == "int4")
  {
    return ParseNumeric<s32, 4>(asset_id, json_value, code_name, value);
  }
  else if (type == "float")
  {
    return ParseNumeric<float, 1>(asset_id, json_value, code_name, value);
  }
  else if (type == "float2")
  {
    return ParseNumeric<float, 2>(asset_id, json_value, code_name, value);
  }
  else if (type == "float3")
  {
    return ParseNumeric<float, 3>(asset_id, json_value, code_name, value);
  }
  else if (type == "float4")
  {
    return ParseNumeric<float, 4>(asset_id, json_value, code_name, value);
  }
  else if (type == "rgb")
  {
    ShaderProperty::RGB rgb;
    if (!ParseNumeric<float, 3>(asset_id, json_value, code_name, &rgb.value))
      return false;
    *value = std::move(rgb);
    return true;
  }
  else if (type == "rgba")
  {
    ShaderProperty::RGBA rgba;
    if (!ParseNumeric<float, 4>(asset_id, json_value, code_name, &rgba.value))
      return false;
    *value = std::move(rgba);
    return true;
  }
  else if (type == "bool")
  {
    if (json_value.is_boolean())
    {
      *value = json_value.get<bool>();
      return true;
    }
  }

  ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse the json, value is not valid for type '{}'",
                asset_id, type);
  return false;
}

static bool
ParseShaderProperties(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                      const nlohmann::json& properties_data,
                      std::map<std::string, VideoCommon::ShaderProperty>* shader_properties)
{
  if (!shader_properties) [[unlikely]]
    return false;

  for (const auto& property_data : properties_data)
  {
    VideoCommon::ShaderProperty property;
    if (!property_data.is_object())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, property is not the right json type",
                    asset_id);
      return false;
    }

    auto type_it = property_data.find("type");
    if (type_it == property_data.end())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, property entry 'type' not found",
                    asset_id);
      return false;
    }
    else if (!type_it->is_string())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, property entry 'type' is not "
                    "the right json type",
                    asset_id);
      return false;
    }
    std::string type = type_it->get<std::string>();
    Common::ToLower(&type);

    auto description_it = property_data.find("description");
    if (description_it == property_data.end())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, property entry 'description' not found",
                    asset_id);
      return false;
    }
    else if (!description_it->is_string())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, property entry 'description' is not "
                    "the right json type",
                    asset_id);
      return false;
    }
    property.description = description_it->get<std::string>();

    auto code_name_it = property_data.find("code_name");
    if (code_name_it == property_data.end())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, property entry 'code_name' not found",
                    asset_id);
      return false;
    }
    else if (!code_name_it->is_string())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, property entry 'code_name' is not "
                    "the right json type",
                    asset_id);
      return false;
    }
    std::string code_name = code_name_it->get<std::string>();

    if (auto default_it = property_data.find("default"); default_it != property_data.end())
    {
      if (!ParseShaderValue(asset_id, *default_it, code_name, type, &property.default_value))
      {
        return false;
      }
    }
    else
    {
      property.default_value = ShaderProperty::GetDefaultValueFromTypeName(type);
    }

    shader_properties->try_emplace(std::move(code_name), std::move(property));
  }

  return true;
}

bool RasterSurfaceShaderData::FromJson(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                                       const nlohmann::json& json, RasterSurfaceShaderData* data)
{
  const auto parse_properties = [&](const char* name, std::string_view source,
                                    std::map<std::string, ShaderProperty>* properties) -> bool {
    if (!json.contains(name))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, '{}' not found", asset_id, name);
      return false;
    }
    if (!json[name].is_array())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, '{}' is not the right json type",
                    asset_id, name);
      return false;
    }

    if (!ParseShaderProperties(asset_id, json, properties))
      return false;

    for (const auto& [code_name, property] : *properties)
    {
      if (source.find(code_name) == std::string::npos)
      {
        ERROR_LOG_FMT(
            VIDEO,
            "Asset '{}' failed to parse json, the code name '{}' defined in the metadata was not "
            "found in the source for '{}'",
            asset_id, code_name, name);
        return false;
      }
    }

    return true;
  };

  const auto parse_samplers =
      [&](const char* name,
          std::vector<VideoCommon::RasterSurfaceShaderData::SamplerData>* samplers) -> bool {
    if (!json.contains(name))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, '{}' not found", asset_id, name);
      return false;
    }
    auto& samplers_array = json[name];
    if (!samplers_array.is_array())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, '{}' is not the right json type",
                    asset_id, name);
      return false;
    }
    if (!std::ranges::all_of(json, &nlohmann::json::is_object))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, '{}' must contain objects", asset_id,
                    name);
      return false;
    }

    for (const auto& sampler_json : samplers_array)
    {
      SamplerData sampler;

      if (const auto sampler_name = ReadStringFromJson(sampler_json, "name"))
      {
        sampler.name = *sampler_name;
      }
      else
      {
        ERROR_LOG_FMT(VIDEO,
                      "Asset '{}' failed to parse sampler json, 'name' not found or wrong type",
                      asset_id);
        return false;
      }

      if (const auto sampler_type = ReadNumericFromJson<AbstractTextureType>(sampler_json, "type"))
      {
        sampler.type = *sampler_type;
      }
      else
      {
        ERROR_LOG_FMT(VIDEO,
                      "Asset '{}' failed to parse sampler json, 'type' not found or wrong type",
                      asset_id);
        return false;
      }
      samplers->push_back(std::move(sampler));
    }

    return true;
  };

  if (!parse_properties("properties", data->pixel_source, &data->uniform_properties))
    return false;

  if (!parse_samplers("samplers", &data->samplers))
    return false;

  return true;
}

void RasterSurfaceShaderData::ToJson(nlohmann::json& obj, const RasterSurfaceShaderData& data)
{
  const auto add_property = [](nlohmann::json* json_properties, const std::string& name,
                               const ShaderProperty& property) {
    nlohmann::json json_property;

    json_property["code_name"] = name;
    json_property["description"] = property.description;

    std::visit(overloaded{[&](s32 default_value) {
                            json_property["type"] = "int";
                            json_property["default"] = default_value;
                          },
                          [&](const std::array<s32, 2>& default_value) {
                            json_property["type"] = "int2";
                            json_property["default"] = default_value;
                          },
                          [&](const std::array<s32, 3>& default_value) {
                            json_property["type"] = "int3";
                            json_property["default"] = default_value;
                          },
                          [&](const std::array<s32, 4>& default_value) {
                            json_property["type"] = "int4";
                            json_property["default"] = default_value;
                          },
                          [&](float default_value) {
                            json_property["type"] = "float";
                            json_property["default"] = default_value;
                          },
                          [&](const std::array<float, 2>& default_value) {
                            json_property["type"] = "float2";
                            json_property["default"] = default_value;
                          },
                          [&](const std::array<float, 3>& default_value) {
                            json_property["type"] = "float3";
                            json_property["default"] = default_value;
                          },
                          [&](const std::array<float, 4>& default_value) {
                            json_property["type"] = "float4";
                            json_property["default"] = default_value;
                          },
                          [&](const ShaderProperty::RGB& default_value) {
                            json_property["type"] = "rgb";
                            json_property["default"] = default_value.value;
                          },
                          [&](const ShaderProperty::RGBA& default_value) {
                            json_property["type"] = "rgba";
                            json_property["default"] = default_value.value;
                          },
                          [&](bool default_value) {
                            json_property["type"] = "bool";
                            json_property["default"] = default_value;
                          }},
               property.default_value);

    json_properties->push_back(std::move(json_property));
  };

  const auto add_sampler = [](nlohmann::json* json_samplers, const SamplerData& sampler) {
    nlohmann::json json_sampler;
    json_sampler["name"] = sampler.name;
    json_sampler["type"] = sampler.type;
    json_samplers->push_back(std::move(json_sampler));
  };

  nlohmann::json json_properties;
  for (const auto& [name, property] : data.uniform_properties)
  {
    add_property(&json_properties, name, property);
  }
  obj["properties"] = std::move(json_properties);

  nlohmann::json json_samplers;
  for (const auto& sampler : data.samplers)
  {
    add_sampler(&json_samplers, sampler);
  }
  obj["samplers"] = json_samplers;
}

std::span<const std::string_view> ShaderProperty::GetValueTypeNames()
{
  static constexpr std::array<std::string_view, 11> values = {
      "int", "int2", "int3", "int4", "float", "float2", "float3", "float4", "rgb", "rgba", "bool"};
  return values;
}

ShaderProperty::Value ShaderProperty::GetDefaultValueFromTypeName(std::string_view name)
{
  if (name == "int")
  {
    return 0;
  }
  else if (name == "int2")
  {
    return std::array<s32, 2>{};
  }
  else if (name == "int3")
  {
    return std::array<s32, 3>{};
  }
  else if (name == "int4")
  {
    return std::array<s32, 4>{};
  }
  else if (name == "float")
  {
    return 0.0f;
  }
  else if (name == "float2")
  {
    return std::array<float, 2>{};
  }
  else if (name == "float3")
  {
    return std::array<float, 3>{};
  }
  else if (name == "float4")
  {
    return std::array<float, 4>{};
  }
  else if (name == "rgb")
  {
    return RGB{};
  }
  else if (name == "rgba")
  {
    return RGBA{};
  }
  else if (name == "bool")
  {
    return false;
  }

  return Value{};
}

void ShaderProperty::WriteAsShaderCode(ShaderCode& shader_source, std::string_view name,
                                       const ShaderProperty& property)
{
  const auto write_shader = [&](std::string_view type, u32 element_count) {
    if (element_count == 1)
    {
      shader_source.Write("{} {};\n", type, name);
    }
    else
    {
      shader_source.Write("{}{} {};\n", type, element_count, name);
    }

    for (std::size_t i = element_count; i < 4; i++)
    {
      shader_source.Write("{} {}_padding_{};\n", type, name, i + 1);
    }
  };
  std::visit(overloaded{[&](s32) { write_shader("int", 1); },
                        [&](const std::array<s32, 2>&) { write_shader("int", 2); },
                        [&](const std::array<s32, 3>&) { write_shader("int", 3); },
                        [&](const std::array<s32, 4>&) { write_shader("int", 4); },
                        [&](float) { write_shader("float", 1); },
                        [&](const std::array<float, 2>&) { write_shader("float", 2); },
                        [&](const std::array<float, 3>&) { write_shader("float", 3); },
                        [&](const std::array<float, 4>&) { write_shader("float", 4); },
                        [&](const ShaderProperty::RGB&) { write_shader("float", 3); },
                        [&](const ShaderProperty::RGBA&) { write_shader("float", 4); },
                        [&](bool) { write_shader("bool", 1); }},
             property.default_value);
}

CustomAssetLibrary::LoadInfo
RasterSurfaceShaderAsset::LoadImpl(const CustomAssetLibrary::AssetID& asset_id)
{
  auto potential_data = std::make_shared<RasterSurfaceShaderData>();
  const auto loaded_info =
      m_owning_library->LoadRasterSurfaceShader(asset_id, potential_data.get());
  if (loaded_info.bytes_loaded == 0)
    return {};
  {
    std::lock_guard lk(m_data_lock);
    m_loaded = true;
    m_data = std::move(potential_data);
  }
  return loaded_info;
}
}  // namespace VideoCommon
