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

    if (!std::ranges::all_of(json_data, &picojson::value::is<double>))
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

template <typename ElementType, std::size_t ElementCount>
bool ParseNumeric(const CustomAssetLibrary::AssetID& asset_id, const picojson::value& json_value,
                  MaterialProperty2::Value* value)
{
  static_assert(ElementCount <= 4, "Numeric data expected to be four elements or less");
  if constexpr (ElementCount == 1)
  {
    if (!json_value.is<double>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' material has attribute where "
                    "a double was expected but not provided.",
                    asset_id);
      return false;
    }

    *value = static_cast<ElementType>(json_value.get<double>());
  }
  else
  {
    if (!json_value.is<picojson::array>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' material has attribute where "
                    "an array was expected but not provided.",
                    asset_id);
      return false;
    }

    const auto json_data = json_value.get<picojson::array>();

    if (json_data.size() != ElementCount)
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' material has attribute with incorrect number "
                    "of elements, expected {}",
                    asset_id, ElementCount);
      return false;
    }

    if (!std::all_of(json_data.begin(), json_data.end(),
                     [](const picojson::value& v) { return v.is<double>(); }))
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' material has attribute where "
                    "all elements are not of type double.",
                    asset_id);
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
                        const picojson::value& json_value, std::string_view type,
                        MaterialProperty2::Value* value)
{
  if (type == "int")
  {
    return ParseNumeric<s32, 1>(asset_id, json_value, value);
  }
  else if (type == "int2")
  {
    return ParseNumeric<s32, 2>(asset_id, json_value, value);
  }
  else if (type == "int3")
  {
    return ParseNumeric<s32, 3>(asset_id, json_value, value);
  }
  else if (type == "int4")
  {
    return ParseNumeric<s32, 4>(asset_id, json_value, value);
  }
  else if (type == "float")
  {
    return ParseNumeric<float, 1>(asset_id, json_value, value);
  }
  else if (type == "float2")
  {
    return ParseNumeric<float, 2>(asset_id, json_value, value);
  }
  else if (type == "float3")
  {
    return ParseNumeric<float, 3>(asset_id, json_value, value);
  }
  else if (type == "float4")
  {
    return ParseNumeric<float, 4>(asset_id, json_value, value);
  }
  else if (type == "bool")
  {
    if (json_value.is<bool>())
    {
      *value = json_value.get<bool>();
      return true;
    }
  }

  ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse the json, value is not valid for type '{}'",
                asset_id, type);
  return false;
}

bool ParseMaterialProperties(const CustomAssetLibrary::AssetID& asset_id,
                             const picojson::array& values_data,
                             std::vector<MaterialProperty2>* material_property)
{
  for (const auto& value_data : values_data)
  {
    VideoCommon::MaterialProperty2 property;
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

    const auto value_iter = value_data_obj.find("value");
    if (value_iter != value_data_obj.end())
    {
      if (!ParsePropertyValue(asset_id, value_iter->second, type, &property.m_value))
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

void MaterialProperty2::WriteToMemory(u8*& buffer, const MaterialProperty2& property)
{
  const auto write_memory = [&](const void* raw_value, std::size_t data_size) {
    std::memcpy(buffer, raw_value, data_size);
    std::memset(buffer + data_size, 0, MemorySize - data_size);
    buffer += MemorySize;
  };
  std::visit(
      overloaded{
          [&](s32 value) { write_memory(&value, sizeof(s32)); },
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

std::size_t MaterialProperty2::GetMemorySize(const MaterialProperty2& property)
{
  std::size_t result = 0;
  std::visit(overloaded{[&](s32 value) { result = MemorySize; },
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

bool RasterMaterialData::FromJson(const CustomAssetLibrary::AssetID& asset_id,
                                  const picojson::object& json, RasterMaterialData* data)
{
  const auto parse_properties = [&](const char* name,
                                    std::vector<MaterialProperty2>* properties) -> bool {
    const auto properties_iter = json.find(name);
    if (properties_iter == json.end())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, '{}' not found", asset_id, name);
      return false;
    }
    if (!properties_iter->second.is<picojson::array>())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, '{}' is not the right json type",
                    asset_id, name);
      return false;
    }
    const auto& properties_array = properties_iter->second.get<picojson::array>();

    if (!ParseMaterialProperties(asset_id, properties_array, properties))
      return false;
    return true;
  };

  if (!parse_properties("vertex_properties", &data->vertex_properties))
    return false;

  if (!parse_properties("pixel_properties", &data->pixel_properties))
    return false;

  if (const auto shader_asset = ReadStringFromJson(json, "shader_asset"))
  {
    data->shader_asset = *shader_asset;
  }
  else
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'shader_asset' not found or wrong type",
                  asset_id);
    return false;
  }

  if (const auto next_material_asset = ReadStringFromJson(json, "next_material_asset"))
  {
    data->next_material_asset = *next_material_asset;
  }
  else
  {
    ERROR_LOG_FMT(VIDEO,
                  "Asset '{}' failed to parse json, 'next_material_asset' not found or wrong type",
                  asset_id);
    return false;
  }

  if (const auto cull_mode = ReadNumericFromJson<CullMode>(json, "cull_mode"))
  {
    data->cull_mode = *cull_mode;
  }

  if (const auto depth_state = json.find("depth_state");
      depth_state != json.end() && depth_state->second.is<picojson::object>())
  {
    auto& json_obj = depth_state->second.get<picojson::object>();
    DepthState state;
    state.testenable = ReadNumericFromJson<u32>(json_obj, "testenable").value_or(0);
    state.updateenable = ReadNumericFromJson<u32>(json_obj, "updateenable").value_or(0);
    state.func = ReadNumericFromJson<CompareMode>(json_obj, "func").value_or(CompareMode::Never);
    data->depth_state = state;
  }

  if (const auto blending_state = json.find("blending_state");
      blending_state != json.end() && blending_state->second.is<picojson::object>())
  {
    auto& json_obj = blending_state->second.get<picojson::object>();
    BlendingState state;
    state.blendenable = ReadNumericFromJson<u32>(json_obj, "blendenable").value_or(0);
    state.logicopenable = ReadNumericFromJson<u32>(json_obj, "logicopenable").value_or(0);
    state.colorupdate = ReadNumericFromJson<u32>(json_obj, "colorupdate").value_or(0);
    state.alphaupdate = ReadNumericFromJson<u32>(json_obj, "alphaupdate").value_or(0);
    state.subtract = ReadNumericFromJson<u32>(json_obj, "subtract").value_or(0);
    state.subtractAlpha = ReadNumericFromJson<u32>(json_obj, "subtractAlpha").value_or(0);
    state.usedualsrc = ReadNumericFromJson<u32>(json_obj, "usedualsrc").value_or(0);
    state.dstfactor =
        ReadNumericFromJson<DstBlendFactor>(json_obj, "dstfactor").value_or(DstBlendFactor::Zero);
    state.srcfactor =
        ReadNumericFromJson<SrcBlendFactor>(json_obj, "srcfactor").value_or(SrcBlendFactor::Zero);
    state.dstfactoralpha = ReadNumericFromJson<DstBlendFactor>(json_obj, "dstfactoralpha")
                               .value_or(DstBlendFactor::Zero);
    state.srcfactoralpha = ReadNumericFromJson<SrcBlendFactor>(json_obj, "srcfactoralpha")
                               .value_or(SrcBlendFactor::Zero);
    state.logicmode = ReadNumericFromJson<LogicOp>(json_obj, "logicmode").value_or(LogicOp::Clear);
    data->blending_state = state;
  }

  const auto pixel_textures_iter = json.find("pixel_textures");
  if (pixel_textures_iter == json.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'pixel_textures' not found", asset_id);
    return false;
  }
  if (!pixel_textures_iter->second.is<picojson::array>())
  {
    ERROR_LOG_FMT(VIDEO,
                  "Asset '{}' failed to parse json, 'pixel_textures' is not the right json type",
                  asset_id);
    return false;
  }
  const auto& pixel_textures_array = pixel_textures_iter->second.get<picojson::array>();
  if (!std::ranges::all_of(pixel_textures_array, [](const picojson::value& json_data) {
        return json_data.is<picojson::object>();
      }))
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'pixel_textures' must contain objects",
                  asset_id);
    return false;
  }

  for (const auto& pixel_texture_json : pixel_textures_array)
  {
    auto& pixel_texture_json_obj = pixel_texture_json.get<picojson::object>();

    TextureSamplerValue sampler_value;
    TextureSamplerValue::FromJson(pixel_texture_json_obj, &sampler_value);
    data->pixel_textures.push_back(std::move(sampler_value));
  }

  return true;
}

void RasterMaterialData::ToJson(picojson::object* obj, const RasterMaterialData& data)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;

  const auto add_property = [](picojson::array* json_properties,
                               const MaterialProperty2& property) {
    picojson::object json_property;

    std::visit(overloaded{[&](s32 value) {
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

    json_properties->emplace_back(std::move(json_property));
  };

  picojson::array json_vertex_properties;
  for (const auto& property : data.vertex_properties)
  {
    add_property(&json_vertex_properties, property);
  }
  json_obj.emplace("vertex_properties", std::move(json_vertex_properties));

  picojson::array json_pixel_properties;
  for (const auto& property : data.pixel_properties)
  {
    add_property(&json_pixel_properties, property);
  }
  json_obj.emplace("pixel_properties", std::move(json_pixel_properties));

  json_obj.emplace("shader_asset", data.shader_asset);
  json_obj.emplace("next_material_asset", data.next_material_asset);

  if (data.cull_mode)
  {
    json_obj.emplace("cull_mode", static_cast<double>(*data.cull_mode));
  }

  if (data.depth_state)
  {
    picojson::object depth_state_json;
    depth_state_json.emplace("testenable",
                             static_cast<double>(data.depth_state->testenable.Value()));
    depth_state_json.emplace("updateenable",
                             static_cast<double>(data.depth_state->updateenable.Value()));
    depth_state_json.emplace("func", static_cast<double>(data.depth_state->func.Value()));
    json_obj.emplace("depth_state", depth_state_json);
  }

  if (data.blending_state)
  {
    picojson::object blending_state_json;
    blending_state_json.emplace("blendenable",
                                static_cast<double>(data.blending_state->blendenable.Value()));
    blending_state_json.emplace("logicopenable",
                                static_cast<double>(data.blending_state->logicopenable.Value()));
    blending_state_json.emplace("colorupdate",
                                static_cast<double>(data.blending_state->colorupdate.Value()));
    blending_state_json.emplace("alphaupdate",
                                static_cast<double>(data.blending_state->alphaupdate.Value()));
    blending_state_json.emplace("subtract",
                                static_cast<double>(data.blending_state->subtract.Value()));
    blending_state_json.emplace("subtractAlpha",
                                static_cast<double>(data.blending_state->subtractAlpha.Value()));
    blending_state_json.emplace("usedualsrc",
                                static_cast<double>(data.blending_state->usedualsrc.Value()));
    blending_state_json.emplace("dstfactor",
                                static_cast<double>(data.blending_state->dstfactor.Value()));
    blending_state_json.emplace("srcfactor",
                                static_cast<double>(data.blending_state->srcfactor.Value()));
    blending_state_json.emplace("dstfactoralpha",
                                static_cast<double>(data.blending_state->dstfactoralpha.Value()));
    blending_state_json.emplace("srcfactoralpha",
                                static_cast<double>(data.blending_state->srcfactoralpha.Value()));
    blending_state_json.emplace("logicmode",
                                static_cast<double>(data.blending_state->logicmode.Value()));
    json_obj.emplace("blending_state", blending_state_json);
  }

  picojson::array json_textures;
  for (const auto& texture : data.pixel_textures)
  {
    picojson::object json_texture;
    TextureSamplerValue::ToJson(&json_texture, texture);
    json_textures.emplace_back(std::move(json_texture));
  }
  json_obj.emplace("pixel_textures", json_textures);
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

CustomAssetLibrary::LoadInfo
RasterMaterialAsset::LoadImpl(const CustomAssetLibrary::AssetID& asset_id)
{
  auto potential_data = std::make_shared<RasterMaterialData>();
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
