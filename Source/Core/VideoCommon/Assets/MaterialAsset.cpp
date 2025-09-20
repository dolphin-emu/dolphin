// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/MaterialAsset.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

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
bool ParseNumeric(const CustomAssetLibrary::AssetID& asset_id, const nlohmann::json& json_value,
                  MaterialProperty::Value* value)
{
  static_assert(ElementCount <= 4, "Numeric data expected to be four elements or less");
  if constexpr (ElementCount == 1)
  {
    if (!json_value.is_number())
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
    if (!json_value.is_array())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' material has attribute where "
                    "an array was expected but not provided.",
                    asset_id);
      return false;
    }

    if (json_value.size() != ElementCount)
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset id '{}' material has attribute with incorrect number "
                    "of elements, expected {}",
                    asset_id, ElementCount);
      return false;
    }

    if (!std::ranges::all_of(json_value, &nlohmann::json::is_number))
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
      data[i] = static_cast<ElementType>(json_value[i].get<double>());
    }
    *value = std::move(data);
  }

  return true;
}
bool ParsePropertyValue(const CustomAssetLibrary::AssetID& asset_id,
                        const nlohmann::json& json_value, std::string_view type,
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

bool ParseMaterialProperties(const CustomAssetLibrary::AssetID& asset_id,
                             const nlohmann::json& values_data,
                             std::vector<MaterialProperty>* material_property)
{
  for (const auto& value_data : values_data)
  {
    VideoCommon::MaterialProperty property;
    if (!value_data.is_object())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse the json, value is not the right json type",
                    asset_id);
      return false;
    }

    auto type_it = value_data.find("type");
    if (type_it == value_data.end())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse the json, value entry 'type' not found",
                    asset_id);
      return false;
    }
    else if (!type_it->is_string())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse the json, value entry 'type' is not "
                    "the right json type",
                    asset_id);
      return false;
    }
    std::string type = type_it->get<std::string>();
    Common::ToLower(&type);

    if (auto value_it = value_data.find("value"); type_it != value_data.end())
    {
      if (!ParsePropertyValue(asset_id, *value_it, type, &property.m_value))
        return false;
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

bool MaterialData::FromJson(const CustomAssetLibrary::AssetID& asset_id, const nlohmann::json& json,
                            MaterialData* data)
{
  const auto parse_properties = [&](const char* name,
                                    std::vector<MaterialProperty>* properties) -> bool {
    if (!json.contains(name))
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, '{}' not found", asset_id, name);
      return false;
    }
    auto& values_data = json[name];
    if (!values_data.is_array())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, '{}' is not the right json type",
                    asset_id, name);
      return false;
    }

    if (!ParseMaterialProperties(asset_id, values_data, properties))
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

  if (const auto json_obj = ReadObjectFromJson(json, "depth_state"))
  {
    DepthState state;
    state.test_enable = ReadNumericFromJson<u32>(*json_obj, "test_enable").value_or(0);
    state.update_enable = ReadNumericFromJson<u32>(*json_obj, "update_enable").value_or(0);
    state.func = ReadNumericFromJson<CompareMode>(*json_obj, "func").value_or(CompareMode::Never);
    data->depth_state = state;
  }

  if (const auto json_obj = ReadObjectFromJson(json, "blending_state"))
  {
    BlendingState state;
    state.blend_enable = ReadNumericFromJson<u32>(*json_obj, "blend_enable").value_or(0);
    state.logic_op_enable = ReadNumericFromJson<u32>(*json_obj, "logic_op_enable").value_or(0);
    state.color_update = ReadNumericFromJson<u32>(*json_obj, "color_update").value_or(0);
    state.alpha_update = ReadNumericFromJson<u32>(*json_obj, "alpha_update").value_or(0);
    state.subtract = ReadNumericFromJson<u32>(*json_obj, "subtract").value_or(0);
    state.subtract_alpha = ReadNumericFromJson<u32>(*json_obj, "subtract_alpha").value_or(0);
    state.use_dual_src = ReadNumericFromJson<u32>(*json_obj, "use_dual_src").value_or(0);
    state.dst_factor =
        ReadNumericFromJson<DstBlendFactor>(*json_obj, "dst_factor").value_or(DstBlendFactor::Zero);
    state.src_factor =
        ReadNumericFromJson<SrcBlendFactor>(*json_obj, "src_factor").value_or(SrcBlendFactor::Zero);
    state.dst_factor_alpha = ReadNumericFromJson<DstBlendFactor>(*json_obj, "dst_factor_alpha")
                                 .value_or(DstBlendFactor::Zero);
    state.src_factor_alpha = ReadNumericFromJson<SrcBlendFactor>(*json_obj, "src_factor_alpha")
                                 .value_or(SrcBlendFactor::Zero);
    state.logic_mode =
        ReadNumericFromJson<LogicOp>(*json_obj, "logic_mode").value_or(LogicOp::Clear);
    data->blending_state = state;
  }

  auto tex_it = json.find("textures");
  if (tex_it == json.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'textures' not found", asset_id);
    return false;
  }
  else if (!tex_it->is_array())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'textures' is not the right json type",
                  asset_id);
    return false;
  }
  auto& textures_array = *tex_it;

  if (!std::ranges::all_of(textures_array, &nlohmann::json::is_object))
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'textures' must contain objects",
                  asset_id);
    return false;
  }

  for (const auto& texture_json : textures_array)
  {
    TextureSamplerValue sampler_value;
    if (!TextureSamplerValue::FromJson(texture_json, &sampler_value))
      return false;
    data->textures.push_back(std::move(sampler_value));
  }

  return true;
}

void MaterialData::ToJson(nlohmann::json* obj, const MaterialData& data)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;

  const auto add_property = [](nlohmann::json* json_properties, const MaterialProperty& property) {
    nlohmann::json json_property;

    std::visit(overloaded{[&](s32 value) {
                            json_property["type"] = "int";
                            json_property["value"] = value;
                          },
                          [&](const std::array<s32, 2>& value) {
                            json_property["type"] = "int2";
                            json_property["value"] = value;
                          },
                          [&](const std::array<s32, 3>& value) {
                            json_property["type"] = "int3";
                            json_property["value"] = value;
                          },
                          [&](const std::array<s32, 4>& value) {
                            json_property["type"] = "int4";
                            json_property["value"] = value;
                          },
                          [&](float value) {
                            json_property["type"] = "float";
                            json_property["value"] = value;
                          },
                          [&](const std::array<float, 2>& value) {
                            json_property["type"] = "float2";
                            json_property["value"] = value;
                          },
                          [&](const std::array<float, 3>& value) {
                            json_property["type"] = "float3";
                            json_property["value"] = value;
                          },
                          [&](const std::array<float, 4>& value) {
                            json_property["type"] = "float4";
                            json_property["value"] = value;
                          },
                          [&](bool value) {
                            json_property["type"] = "bool";
                            json_property["value"] = value;
                          }},
               property.m_value);

    json_properties->push_back(std::move(json_property));
  };

  nlohmann::json json_properties;
  for (const auto& property : data.properties)
  {
    add_property(&json_properties, property);
  }
  json_obj["properties"] = std::move(json_properties);

  json_obj["shader_asset"] = data.shader_asset;
  json_obj["next_material_asset"] = data.next_material_asset;

  if (data.cull_mode)
    json_obj["cull_mode"] = *data.cull_mode;

  if (data.depth_state)
  {
    nlohmann::json depth_state_json;
    depth_state_json["test_enable"] = data.depth_state->test_enable.Value();
    depth_state_json["update_enable"] = data.depth_state->update_enable.Value();
    depth_state_json["func"] = data.depth_state->func.Value();
    json_obj["depth_state"] = depth_state_json;
  }

  if (data.blending_state)
  {
    nlohmann::json blending_state_json;
    blending_state_json["blend_enable"] = data.blending_state->blend_enable.Value();
    blending_state_json["logic_op_enable"] = data.blending_state->logic_op_enable.Value();
    blending_state_json["color_update"] = data.blending_state->color_update.Value();
    blending_state_json["alpha_update"] = data.blending_state->alpha_update.Value();
    blending_state_json["subtract"] = data.blending_state->subtract.Value();
    blending_state_json["subtract_alpha"] = data.blending_state->subtract_alpha.Value();
    blending_state_json["use_dual_src"] = data.blending_state->use_dual_src.Value();
    blending_state_json["dst_factor"] = data.blending_state->dst_factor.Value();
    blending_state_json["src_factor"] = data.blending_state->src_factor.Value();
    blending_state_json["dst_factor_alpha"] = data.blending_state->dst_factor_alpha.Value();
    blending_state_json["src_factor_alpha"] = data.blending_state->src_factor_alpha.Value();
    blending_state_json["logic_mode"] = data.blending_state->logic_mode.Value();
    json_obj["blending_state"] = blending_state_json;
  }

  nlohmann::json json_textures;
  for (const auto& texture : data.textures)
  {
    nlohmann::json json_texture;
    TextureSamplerValue::ToJson(&json_texture, texture);
    json_textures.push_back(std::move(json_texture));
  }
  json_obj["textures"] = json_textures;
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
