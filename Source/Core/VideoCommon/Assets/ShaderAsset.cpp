// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/ShaderAsset.h"

#include "Common/Logging/Log.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"

namespace VideoCommon
{
bool ParseShaderProperties(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
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
    std::transform(type.begin(), type.end(), type.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (type == "sampler2d")
    {
      property.m_type = ShaderProperty::Type::Type_Sampler2D;
    }
    else if (type == "samplercube")
    {
      property.m_type = ShaderProperty::Type::Type_SamplerCube;
    }
    else if (type == "samplerarrayshared_main")
    {
      property.m_type = ShaderProperty::Type::Type_SamplerArrayShared_Main;
    }
    else if (type == "samplerarrayshared_additional")
    {
      property.m_type = ShaderProperty::Type::Type_SamplerArrayShared_Additional;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, property entry 'description' is "
                    "an invalid option",
                    asset_id);
      return false;
    }

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
    shader_properties->try_emplace(code_name_iter->second.to_str(), std::move(property));
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
