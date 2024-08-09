// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/ShaderAsset.h"

#include <algorithm>
#include <utility>

#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/TextureSamplerValue.h"

namespace VideoCommon
{
template <typename ElementType, std::size_t ElementCount, typename PropertyType>
bool ParseNumeric(const CustomAssetLibrary::AssetID& asset_id, const picojson::value& json_value,
                  std::string_view code_name, PropertyType* value)
{
  static_assert(ElementCount <= 4, "Numeric data expected to be four elements or less");
  if constexpr (ElementCount == 1)
  {
    if (!json_value.is<double>())
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
    if (!json_value.is<picojson::array>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' shader has attribute '{}' where "
                    "an array was expected but not provided.",
                    asset_id, code_name);
      return false;
    }

    const auto json_data = json_value.get<picojson::array>();

    if (json_data.size() != ElementCount)
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' shader has attribute '{}' with incorrect number "
                    "of elements, expected {}",
                    asset_id, code_name, ElementCount);
      return false;
    }

    if (!std::all_of(json_data.begin(), json_data.end(),
                     [](const picojson::value& v) { return v.is<double>(); }))
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
      data[i] = static_cast<ElementType>(json_data[i].get<double>());
    }
    *value = std::move(data);
  }

  return true;
}

static bool ParseShaderValue(const CustomAssetLibrary::AssetID& asset_id,
                             const picojson::value& json_value, std::string_view code_name,
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
    if (json_value.is<bool>())
    {
      *value = json_value.get<bool>();
      return true;
    }
  }
  else if (type == "sampler2d")
  {
    if (json_value.is<std::string>())
    {
      ShaderProperty::Sampler2D sampler2d;
      sampler2d.value.asset = json_value.get<std::string>();
      *value = sampler2d;
      return true;
    }
    else if (json_value.is<picojson::object>())
    {
      ShaderProperty::Sampler2D sampler2d;
      auto texture_asset_obj = json_value.get<picojson::object>();
      if (!TextureSamplerValue::FromJson(texture_asset_obj, &sampler2d.value))
        return false;
      *value = sampler2d;
      return true;
    }
  }
  else if (type == "sampler2darray")
  {
    if (json_value.is<std::string>())
    {
      ShaderProperty::Sampler2DArray sampler2darray;
      sampler2darray.value.asset = json_value.get<std::string>();
      *value = sampler2darray;
      return true;
    }
    else if (json_value.is<picojson::object>())
    {
      ShaderProperty::Sampler2DArray sampler2darray;
      auto texture_asset_obj = json_value.get<picojson::object>();
      if (!TextureSamplerValue::FromJson(texture_asset_obj, &sampler2darray.value))
        return false;
      *value = sampler2darray;
      return true;
    }
  }
  else if (type == "samplercube")
  {
    if (json_value.is<std::string>())
    {
      ShaderProperty::SamplerCube samplercube;
      samplercube.value.asset = json_value.get<std::string>();
      *value = samplercube;
      return true;
    }
    else if (json_value.is<picojson::object>())
    {
      ShaderProperty::SamplerCube samplercube;
      auto texture_asset_obj = json_value.get<picojson::object>();
      if (!TextureSamplerValue::FromJson(texture_asset_obj, &samplercube.value))
        return false;
      *value = samplercube;
      return true;
    }
  }

  ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse the json, value is not valid for type '{}'",
                asset_id, type);
  return false;
}

static bool
ParseShaderProperties(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                      const picojson::array& properties_data,
                      std::map<std::string, VideoCommon::ShaderProperty>* shader_properties)
{
  if (!shader_properties) [[unlikely]]
    return false;

  for (const auto& property_data : properties_data)
  {
    VideoCommon::ShaderProperty property;
    if (!property_data.is<picojson::object>())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, property is not the right json type",
                    asset_id);
      return false;
    }
    const auto& property_data_obj = property_data.get<picojson::object>();

    const auto type_iter = property_data_obj.find("type");
    if (type_iter == property_data_obj.end())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, property entry 'type' not found",
                    asset_id);
      return false;
    }
    if (!type_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, property entry 'type' is not "
                    "the right json type",
                    asset_id);
      return false;
    }
    std::string type = type_iter->second.to_str();
    Common::ToLower(&type);

    const auto description_iter = property_data_obj.find("description");
    if (description_iter == property_data_obj.end())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, property entry 'description' not found",
                    asset_id);
      return false;
    }
    if (!description_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, property entry 'description' is not "
                    "the right json type",
                    asset_id);
      return false;
    }
    property.m_description = description_iter->second.to_str();

    const auto code_name_iter = property_data_obj.find("code_name");
    if (code_name_iter == property_data_obj.end())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, property entry 'code_name' not found",
                    asset_id);
      return false;
    }
    if (!code_name_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, property entry 'code_name' is not "
                    "the right json type",
                    asset_id);
      return false;
    }
    std::string code_name = code_name_iter->second.to_str();

    const auto default_iter = property_data_obj.find("default");
    if (default_iter != property_data_obj.end())
    {
      if (!ParseShaderValue(asset_id, default_iter->second, code_name, type, &property.m_default))
      {
        return false;
      }
    }
    else
    {
      property.m_default = ShaderProperty::GetDefaultValueFromTypeName(type);
    }

    shader_properties->try_emplace(std::move(code_name), std::move(property));
  }

  return true;
}

bool PixelShaderData::FromJson(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                               const picojson::object& json, PixelShaderData* data)
{
  const auto properties_iter = json.find("properties");
  if (properties_iter == json.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'properties' not found", asset_id);
    return false;
  }
  if (!properties_iter->second.is<picojson::array>())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'properties' is not the right json type",
                  asset_id);
    return false;
  }
  const auto& properties_array = properties_iter->second.get<picojson::array>();
  if (!ParseShaderProperties(asset_id, properties_array, &data->m_properties))
    return false;

  for (const auto& [name, property] : data->m_properties)
  {
    if (data->m_shader_source.find(name) == std::string::npos)
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Asset '{}' failed to parse json, the code name '{}' defined in the metadata was not "
          "found in the shader source",
          asset_id, name);
      return false;
    }
  }

  return true;
}

void PixelShaderData::ToJson(picojson::object& obj, const PixelShaderData& data)
{
  picojson::array json_properties;
  for (const auto& [name, property] : data.m_properties)
  {
    picojson::object json_property;
    json_property.emplace("code_name", name);
    json_property.emplace("description", property.m_description);

    std::visit(overloaded{[&](const ShaderProperty::Sampler2D& default_value) {
                            json_property.emplace("type", "sampler2d");
                            picojson::object texture_sampler_obj;
                            TextureSamplerValue::ToJson(&texture_sampler_obj, default_value.value);
                            json_property.emplace("default", picojson::value{texture_sampler_obj});
                          },
                          [&](const ShaderProperty::Sampler2DArray& default_value) {
                            json_property.emplace("type", "sampler2darray");
                            picojson::object texture_sampler_obj;
                            TextureSamplerValue::ToJson(&texture_sampler_obj, default_value.value);
                            json_property.emplace("default", picojson::value{texture_sampler_obj});
                          },
                          [&](const ShaderProperty::SamplerCube& default_value) {
                            json_property.emplace("type", "samplercube");
                            picojson::object texture_sampler_obj;
                            TextureSamplerValue::ToJson(&texture_sampler_obj, default_value.value);
                            json_property.emplace("default", picojson::value{texture_sampler_obj});
                          },
                          [&](s32 default_value) {
                            json_property.emplace("type", "int");
                            json_property.emplace("default", static_cast<double>(default_value));
                          },
                          [&](const std::array<s32, 2>& default_value) {
                            json_property.emplace("type", "int2");
                            json_property.emplace("default", ToJsonArray(default_value));
                          },
                          [&](const std::array<s32, 3>& default_value) {
                            json_property.emplace("type", "int3");
                            json_property.emplace("default", ToJsonArray(default_value));
                          },
                          [&](const std::array<s32, 4>& default_value) {
                            json_property.emplace("type", "int4");
                            json_property.emplace("default", ToJsonArray(default_value));
                          },
                          [&](float default_value) {
                            json_property.emplace("type", "float");
                            json_property.emplace("default", static_cast<double>(default_value));
                          },
                          [&](const std::array<float, 2>& default_value) {
                            json_property.emplace("type", "float2");
                            json_property.emplace("default", ToJsonArray(default_value));
                          },
                          [&](const std::array<float, 3>& default_value) {
                            json_property.emplace("type", "float3");
                            json_property.emplace("default", ToJsonArray(default_value));
                          },
                          [&](const std::array<float, 4>& default_value) {
                            json_property.emplace("type", "float4");
                            json_property.emplace("default", ToJsonArray(default_value));
                          },
                          [&](const ShaderProperty::RGB& default_value) {
                            json_property.emplace("type", "rgb");
                            json_property.emplace("default", ToJsonArray(default_value.value));
                          },
                          [&](const ShaderProperty::RGBA& default_value) {
                            json_property.emplace("type", "rgba");
                            json_property.emplace("default", ToJsonArray(default_value.value));
                          },
                          [&](bool default_value) {
                            json_property.emplace("type", "bool");
                            json_property.emplace("default", default_value);
                          }},
               property.m_default);

    json_properties.emplace_back(std::move(json_property));
  }

  obj.emplace("properties", std::move(json_properties));
}

std::span<const std::string_view> ShaderProperty::GetValueTypeNames()
{
  static constexpr std::array<std::string_view, 14> values = {
      "sampler2d", "sampler2darray", "samplercube", "int",    "int2", "int3", "int4",
      "float",     "float2",         "float3",      "float4", "rgb",  "rgba", "bool"};
  return values;
}

ShaderProperty::Value ShaderProperty::GetDefaultValueFromTypeName(std::string_view name)
{
  if (name == "sampler2d")
  {
    return Sampler2D{};
  }
  else if (name == "sampler2darray")
  {
    return Sampler2DArray{};
  }
  else if (name == "samplercube")
  {
    return SamplerCube{};
  }
  else if (name == "int")
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

CustomAssetLibrary::LoadInfo PixelShaderAsset::LoadImpl(const CustomAssetLibrary::AssetID& asset_id)
{
  auto potential_data = std::make_shared<PixelShaderData>();
  const auto loaded_info = m_owning_library->LoadPixelShader(asset_id, potential_data.get());
  if (loaded_info.m_bytes_loaded == 0)
    return {};
  {
    std::lock_guard lk(m_data_lock);
    m_loaded = true;
    m_data = std::move(potential_data);
  }
  return loaded_info;
}
}  // namespace VideoCommon
