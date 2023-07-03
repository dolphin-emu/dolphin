// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/MaterialAsset.h"

#include <vector>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"

namespace VideoCommon
{
namespace
{
bool ParsePropertyValue(const CustomAssetLibrary::AssetID& asset_id, MaterialProperty::Type type,
                        const picojson::value& json_value,
                        std::optional<MaterialProperty::Value>* value)
{
  switch (type)
  {
  case MaterialProperty::Type::Type_TextureAsset:
  {
    if (json_value.is<std::string>())
    {
      *value = json_value.to_str();
      return true;
    }
  }
  break;
  };

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
    if (type == "texture_asset")
    {
      property.m_type = MaterialProperty::Type::Type_TextureAsset;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse the json, value entry 'type' is "
                    "an invalid option",
                    asset_id);
      return false;
    }

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
      if (!ParsePropertyValue(asset_id, property.m_type, value_iter->second, &property.m_value))
        return false;
    }

    material_property->push_back(std::move(property));
  }

  return true;
}
}  // namespace
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
