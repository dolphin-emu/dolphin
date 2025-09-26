// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModAsset.h"

#include <nlohmann/json.hpp>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

void GraphicsModAssetConfig::SerializeToConfig(nlohmann::json& json_obj) const
{
  json_obj["name"] = m_asset_id;

  nlohmann::json serialized_data;
  for (const auto& [name, path] : m_map)
  {
    serialized_data[name] = PathToString(path);
  }
  json_obj["data"] = std::move(serialized_data);
}

bool GraphicsModAssetConfig::DeserializeFromConfig(const nlohmann::json& obj)
{
  auto name_it = obj.find("name");
  if (name_it == obj.end())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load mod configuration file, specified asset has no name");
    return false;
  }
  else if (!name_it->is_string())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load mod configuration file, specified asset has a name "
                         "that is not a string");
    return false;
  }
  m_asset_id = name_it->get<std::string>();

  auto data_it = obj.find("data");
  if (data_it == obj.end())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load mod configuration file, specified asset '{}' has no data",
                  m_asset_id);
    return false;
  }
  else if (!data_it->is_object())
  {
    ERROR_LOG_FMT(VIDEO,
                  "Failed to load mod configuration file, specified asset '{}' has data "
                  "that is not an object",
                  m_asset_id);
    return false;
  }
  for (const auto& [key, value] : data_it->items())
  {
    if (!value.is_string())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load mod configuration file, specified asset '{}' has data "
                    "with a value for key '{}' that is not a string",
                    m_asset_id, key);
      return false;
    }
    m_map[key] = value.get<std::string>();
  }

  return true;
}
