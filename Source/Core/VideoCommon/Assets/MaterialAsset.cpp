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
                  MaterialProperty::Value* value)
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

    if (!std::ranges::all_of(json_data, &picojson::value::is<double>))
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
                        MaterialProperty::Value* value)
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

std::size_t MaterialProperty::GetMemorySize(const MaterialProperty& property)
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
  const auto parse_properties = [&](const char* name,
                                    std::vector<MaterialProperty>* material_properties) -> bool {
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

    if (!ParseMaterialProperties(asset_id, properties_array, material_properties))
      return false;
    return true;
  };

  if (!parse_properties("properties", &data->properties))
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
    state.test_enable = ReadNumericFromJson<u32>(json_obj, "test_enable").value_or(0);
    state.update_enable = ReadNumericFromJson<u32>(json_obj, "update_enable").value_or(0);
    state.func = ReadNumericFromJson<CompareMode>(json_obj, "func").value_or(CompareMode::Never);
    data->depth_state = state;
  }

  if (const auto blending_state = json.find("blending_state");
      blending_state != json.end() && blending_state->second.is<picojson::object>())
  {
    auto& json_obj = blending_state->second.get<picojson::object>();
    BlendingState state;
    state.blend_enable = ReadNumericFromJson<u32>(json_obj, "blend_enable").value_or(0);
    state.logic_op_enable = ReadNumericFromJson<u32>(json_obj, "logic_op_enable").value_or(0);
    state.color_update = ReadNumericFromJson<u32>(json_obj, "color_update").value_or(0);
    state.alpha_update = ReadNumericFromJson<u32>(json_obj, "alpha_update").value_or(0);
    state.subtract = ReadNumericFromJson<u32>(json_obj, "subtract").value_or(0);
    state.subtract_alpha = ReadNumericFromJson<u32>(json_obj, "subtract_alpha").value_or(0);
    state.use_dual_src = ReadNumericFromJson<u32>(json_obj, "use_dual_src").value_or(0);
    state.dst_factor =
        ReadNumericFromJson<DstBlendFactor>(json_obj, "dst_factor").value_or(DstBlendFactor::Zero);
    state.src_factor =
        ReadNumericFromJson<SrcBlendFactor>(json_obj, "src_factor").value_or(SrcBlendFactor::Zero);
    state.dst_factor_alpha = ReadNumericFromJson<DstBlendFactor>(json_obj, "dst_factor_alpha")
                                 .value_or(DstBlendFactor::Zero);
    state.src_factor_alpha = ReadNumericFromJson<SrcBlendFactor>(json_obj, "src_factor_alpha")
                                 .value_or(SrcBlendFactor::Zero);
    state.logic_mode =
        ReadNumericFromJson<LogicOp>(json_obj, "logic_mode").value_or(LogicOp::Clear);
    data->blending_state = state;
  }

  const auto textures_iter = json.find("textures");
  if (textures_iter == json.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'textures' not found", asset_id);
    return false;
  }
  if (!textures_iter->second.is<picojson::array>())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'textures' is not the right json type",
                  asset_id);
    return false;
  }
  const auto& textures_array = textures_iter->second.get<picojson::array>();
  if (!std::ranges::all_of(textures_array, [](const picojson::value& json_data) {
        return json_data.is<picojson::object>();
      }))
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'textures' must contain objects",
                  asset_id);
    return false;
  }

  for (const auto& texture_json : textures_array)
  {
    auto& texture_json_obj = texture_json.get<picojson::object>();

    TextureSamplerValue sampler_value;
    if (!TextureSamplerValue::FromJson(texture_json_obj, &sampler_value))
      return false;
    data->textures.push_back(std::move(sampler_value));
  }

  return true;
}

void MaterialData::ToJson(picojson::object* obj, const MaterialData& data)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;

  const auto add_property = [](picojson::array* json_properties, const MaterialProperty& property) {
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

  picojson::array json_properties;
  for (const auto& property : data.properties)
  {
    add_property(&json_properties, property);
  }
  json_obj.emplace("properties", std::move(json_properties));

  json_obj.emplace("shader_asset", data.shader_asset);
  json_obj.emplace("next_material_asset", data.next_material_asset);

  if (data.cull_mode)
  {
    json_obj.emplace("cull_mode", static_cast<double>(*data.cull_mode));
  }

  if (data.depth_state)
  {
    picojson::object depth_state_json;
    depth_state_json.emplace("test_enable",
                             static_cast<double>(data.depth_state->test_enable.Value()));
    depth_state_json.emplace("update_enable",
                             static_cast<double>(data.depth_state->update_enable.Value()));
    depth_state_json.emplace("func", static_cast<double>(data.depth_state->func.Value()));
    json_obj.emplace("depth_state", depth_state_json);
  }

  if (data.blending_state)
  {
    picojson::object blending_state_json;
    blending_state_json.emplace("blend_enable",
                                static_cast<double>(data.blending_state->blend_enable.Value()));
    blending_state_json.emplace("logic_op_enable",
                                static_cast<double>(data.blending_state->logic_op_enable.Value()));
    blending_state_json.emplace("color_update",
                                static_cast<double>(data.blending_state->color_update.Value()));
    blending_state_json.emplace("alpha_update",
                                static_cast<double>(data.blending_state->alpha_update.Value()));
    blending_state_json.emplace("subtract",
                                static_cast<double>(data.blending_state->subtract.Value()));
    blending_state_json.emplace("subtract_alpha",
                                static_cast<double>(data.blending_state->subtract_alpha.Value()));
    blending_state_json.emplace("use_dual_src",
                                static_cast<double>(data.blending_state->use_dual_src.Value()));
    blending_state_json.emplace("dst_factor",
                                static_cast<double>(data.blending_state->dst_factor.Value()));
    blending_state_json.emplace("src_factor",
                                static_cast<double>(data.blending_state->src_factor.Value()));
    blending_state_json.emplace("dst_factor_alpha",
                                static_cast<double>(data.blending_state->dst_factor_alpha.Value()));
    blending_state_json.emplace("src_factor_alpha",
                                static_cast<double>(data.blending_state->src_factor_alpha.Value()));
    blending_state_json.emplace("logic_mode",
                                static_cast<double>(data.blending_state->logic_mode.Value()));
    json_obj.emplace("blending_state", blending_state_json);
  }

  picojson::array json_textures;
  for (const auto& texture : data.textures)
  {
    picojson::object json_texture;
    TextureSamplerValue::ToJson(&json_texture, texture);
    json_textures.emplace_back(std::move(json_texture));
  }
  json_obj.emplace("textures", json_textures);
}

CustomAssetLibrary::LoadInfo MaterialAsset::LoadImpl(const CustomAssetLibrary::AssetID& asset_id)
{
  auto potential_data = std::make_shared<MaterialData>();
  const auto loaded_info = m_owning_library->LoadMaterial(asset_id, potential_data.get());
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
