// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/MaterialAsset.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/ShaderGenCommon.h"

namespace VideoCommon
{
namespace
{
// While not optimal, we pad our data to match std140 shader requirements
// this memory constant indicates the memory stride for a single uniform
// regardless of data type
constexpr std::size_t MemorySize = sizeof(float) * 4;

template <typename ElementType, std::size_t ElementCount>
bool ParseNumeric(const CustomAssetLibrary::AssetID& asset_id, const picojson::value& json_value,
                  std::string_view code_name, MaterialProperty::Value* value)
{
  static_assert(ElementCount <= 4, "Numeric data expected to be four elements or less");
  if constexpr (ElementCount == 1)
  {
    if (!json_value.is<double>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' material has attribute '{}' where "
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
                    "Asset id '{}' material has attribute '{}' where "
                    "an array was expected but not provided.",
                    asset_id, code_name);
      return false;
    }

    const auto json_data = json_value.get<picojson::array>();

    if (json_data.size() != ElementCount)
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' material has attribute '{}' with incorrect number "
                    "of elements, expected {}",
                    asset_id, code_name, ElementCount);
      return false;
    }

    if (!std::all_of(json_data.begin(), json_data.end(),
                     [](const picojson::value& v) { return v.is<double>(); }))
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' material has attribute '{}' where "
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
bool ParsePropertyValue(const CustomAssetLibrary::AssetID& asset_id,
                        const picojson::value& json_value, std::string_view code_name,
                        std::string_view type, MaterialProperty::Value* value)
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
  else if (type == "bool")
  {
    if (json_value.is<bool>())
    {
      *value = json_value.get<bool>();
      return true;
    }
  }
  else if (type == "texture_asset")
  {
    if (json_value.is<std::string>())
    {
      TextureSamplerValue texture_sampler_value;
      texture_sampler_value.asset = json_value.get<std::string>();
      *value = texture_sampler_value;
      return true;
    }
    else if (json_value.is<picojson::object>())
    {
      auto texture_asset_obj = json_value.get<picojson::object>();
      TextureSamplerValue texture_sampler_value;
      if (!TextureSamplerValue::FromJson(texture_asset_obj, &texture_sampler_value))
        return false;
      *value = texture_sampler_value;
      return true;
    }
  }

  ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse the json, value is not valid for type '{}'",
                asset_id, type);
  return false;
}

bool ParseMaterialProperties(const CustomAssetLibrary::AssetID& asset_id,
                             const picojson::array& values_data,
                             std::vector<MaterialProperty>* material_property)
{
  for (const auto& value_data : values_data)
  {
    VideoCommon::MaterialProperty property;
    if (!value_data.is<picojson::object>())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse the json, value is not the right json type",
                    asset_id);
      return false;
    }
    const auto& value_data_obj = value_data.get<picojson::object>();

    const auto type_iter = value_data_obj.find("type");
    if (type_iter == value_data_obj.end())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse the json, value entry 'type' not found",
                    asset_id);
      return false;
    }
    if (!type_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse the json, value entry 'type' is not "
                    "the right json type",
                    asset_id);
      return false;
    }
    std::string type = type_iter->second.to_str();
    Common::ToLower(&type);

    const auto code_name_iter = value_data_obj.find("code_name");
    if (code_name_iter == value_data_obj.end())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse the json, value entry "
                    "'code_name' not found",
                    asset_id);
      return false;
    }
    if (!code_name_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse the json, value entry 'code_name' is not "
                    "the right json type",
                    asset_id);
      return false;
    }
    property.m_code_name = code_name_iter->second.to_str();

    const auto value_iter = value_data_obj.find("value");
    if (value_iter != value_data_obj.end())
    {
      if (!ParsePropertyValue(asset_id, value_iter->second, property.m_code_name, type,
                              &property.m_value))
      {
        return false;
      }
    }

    material_property->push_back(std::move(property));
  }

  return true;
}
}  // namespace

void MaterialProperty::WriteToMemory(u8*& buffer, const MaterialProperty& property)
{
  const auto write_memory = [&](const void* raw_value, std::size_t data_size) {
    std::memcpy(buffer, raw_value, data_size);
    std::memset(buffer + data_size, 0, MemorySize - data_size);
    buffer += MemorySize;
  };
  std::visit(
      overloaded{
          [&](const TextureSamplerValue&) {}, [&](s32 value) { write_memory(&value, sizeof(s32)); },
          [&](const std::array<s32, 2>& value) { write_memory(value.data(), sizeof(s32) * 2); },
          [&](const std::array<s32, 3>& value) { write_memory(value.data(), sizeof(s32) * 3); },
          [&](const std::array<s32, 4>& value) { write_memory(value.data(), sizeof(s32) * 4); },
          [&](float value) { write_memory(&value, sizeof(float)); },
          [&](const std::array<float, 2>& value) { write_memory(value.data(), sizeof(float) * 2); },
          [&](const std::array<float, 3>& value) { write_memory(value.data(), sizeof(float) * 3); },
          [&](const std::array<float, 4>& value) { write_memory(value.data(), sizeof(float) * 4); },
          [&](bool value) { write_memory(&value, sizeof(bool)); }},
      property.m_value);
}

std::size_t MaterialProperty::GetMemorySize(const MaterialProperty& property)
{
  std::size_t result = 0;
  std::visit(overloaded{[&](const TextureSamplerValue&) {}, [&](s32) { result = MemorySize; },
                        [&](const std::array<s32, 2>&) { result = MemorySize; },
                        [&](const std::array<s32, 3>&) { result = MemorySize; },
                        [&](const std::array<s32, 4>&) { result = MemorySize; },
                        [&](float) { result = MemorySize; },
                        [&](const std::array<float, 2>&) { result = MemorySize; },
                        [&](const std::array<float, 3>&) { result = MemorySize; },
                        [&](const std::array<float, 4>&) { result = MemorySize; },
                        [&](bool) { result = MemorySize; }},
             property.m_value);

  return result;
}

void MaterialProperty::WriteAsShaderCode(ShaderCode& shader_source,
                                         const MaterialProperty& property)
{
  const auto write_shader = [&](std::string_view type, u32 element_count) {
    if (element_count == 1)
    {
      shader_source.Write("{} {};\n", type, property.m_code_name);
    }
    else
    {
      shader_source.Write("{}{} {};\n", type, element_count, property.m_code_name);
    }

    for (std::size_t i = element_count; i < 4; i++)
    {
      shader_source.Write("{} {}_padding_{};\n", type, property.m_code_name, i + 1);
    }
  };
  std::visit(overloaded{[&](const TextureSamplerValue&) {},
                        [&](s32 value) { write_shader("int", 1); },
                        [&](const std::array<s32, 2>& value) { write_shader("int", 2); },
                        [&](const std::array<s32, 3>& value) { write_shader("int", 3); },
                        [&](const std::array<s32, 4>& value) { write_shader("int", 4); },
                        [&](float value) { write_shader("float", 1); },
                        [&](const std::array<float, 2>& value) { write_shader("float", 2); },
                        [&](const std::array<float, 3>& value) { write_shader("float", 3); },
                        [&](const std::array<float, 4>& value) { write_shader("float", 4); },
                        [&](bool value) { write_shader("bool", 1); }},
             property.m_value);
}

bool MaterialData::FromJson(const CustomAssetLibrary::AssetID& asset_id,
                            const picojson::object& json, MaterialData* data)
{
  const auto values_iter = json.find("values");
  if (values_iter == json.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'values' not found", asset_id);
    return false;
  }
  if (!values_iter->second.is<picojson::array>())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'values' is not the right json type",
                  asset_id);
    return false;
  }
  const auto& values_array = values_iter->second.get<picojson::array>();

  if (!ParseMaterialProperties(asset_id, values_array, &data->properties))
    return false;

  const auto shader_asset_iter = json.find("shader_asset");
  if (shader_asset_iter == json.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'shader_asset' not found", asset_id);
    return false;
  }
  if (!shader_asset_iter->second.is<std::string>())
  {
    ERROR_LOG_FMT(VIDEO,
                  "Asset '{}' failed to parse json, 'shader_asset' is not the right json type",
                  asset_id);
    return false;
  }
  data->shader_asset = shader_asset_iter->second.to_str();

  return true;
}

void MaterialData::ToJson(picojson::object* obj, const MaterialData& data)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;

  picojson::array json_properties;
  for (const auto& property : data.properties)
  {
    picojson::object json_property;
    json_property["code_name"] = picojson::value{property.m_code_name};

    std::visit(overloaded{[&](const TextureSamplerValue& value) {
                            json_property["type"] = picojson::value{"texture_asset"};
                            picojson::object value_json;
                            TextureSamplerValue::ToJson(&value_json, value);
                            json_property["value"] = picojson::value{value_json};
                          },
                          [&](s32 value) {
                            json_property["type"] = picojson::value{"int"};
                            json_property["value"] = picojson::value{static_cast<double>(value)};
                          },
                          [&](const std::array<s32, 2>& value) {
                            json_property["type"] = picojson::value{"int2"};
                            json_property["value"] = picojson::value{ToJsonArray(value)};
                          },
                          [&](const std::array<s32, 3>& value) {
                            json_property["type"] = picojson::value{"int3"};
                            json_property["value"] = picojson::value{ToJsonArray(value)};
                          },
                          [&](const std::array<s32, 4>& value) {
                            json_property["type"] = picojson::value{"int4"};
                            json_property["value"] = picojson::value{ToJsonArray(value)};
                          },
                          [&](float value) {
                            json_property["type"] = picojson::value{"float"};
                            json_property["value"] = picojson::value{static_cast<double>(value)};
                          },
                          [&](const std::array<float, 2>& value) {
                            json_property["type"] = picojson::value{"float2"};
                            json_property["value"] = picojson::value{ToJsonArray(value)};
                          },
                          [&](const std::array<float, 3>& value) {
                            json_property["type"] = picojson::value{"float3"};
                            json_property["value"] = picojson::value{ToJsonArray(value)};
                          },
                          [&](const std::array<float, 4>& value) {
                            json_property["type"] = picojson::value{"float4"};
                            json_property["value"] = picojson::value{ToJsonArray(value)};
                          },
                          [&](bool value) {
                            json_property["type"] = picojson::value{"bool"};
                            json_property["value"] = picojson::value{value};
                          }},
               property.m_value);

    json_properties.emplace_back(std::move(json_property));
  }

  json_obj["values"] = picojson::value{std::move(json_properties)};
  json_obj["shader_asset"] = picojson::value{data.shader_asset};
}

CustomAssetLibrary::LoadInfo MaterialAsset::LoadImpl(const CustomAssetLibrary::AssetID& asset_id)
{
  auto potential_data = std::make_shared<MaterialData>();
  const auto loaded_info = m_owning_library->LoadMaterial(asset_id, potential_data.get());
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
