// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModAsset.h"

#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace GraphicsModSystem::Config
{
void GraphicsModAsset::Serialize(picojson::object& json_obj) const
{
  json_obj.emplace("id", m_asset_id);

  picojson::object serialized_data;
  for (const auto& [name, path] : m_map)
  {
    serialized_data.emplace(name, PathToString(path));
  }
  json_obj.emplace("data", std::move(serialized_data));
}

bool GraphicsModAsset::Deserialize(const picojson::object& obj)
{
  const auto id = ReadStringFromJson(obj, "id");
  if (!id)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load mod configuration file, specified asset has an id "
                         "that is not valid");
    return false;
  }
  m_asset_id = *id;

  auto data_iter = obj.find("data");
  if (data_iter == obj.end())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load mod configuration file, specified asset '{}' has no data",
                  m_asset_id);
    return false;
  }
  if (!data_iter->second.is<picojson::object>())
  {
    ERROR_LOG_FMT(VIDEO,
                  "Failed to load mod configuration file, specified asset '{}' has data "
                  "that is not an object",
                  m_asset_id);
    return false;
  }
  for (const auto& [key, value] : data_iter->second.get<picojson::object>())
  {
    if (!value.is<std::string>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load mod configuration file, specified asset '{}' has data "
                    "with a value for key '{}' that is not a string",
                    m_asset_id, key);
      return false;
    }
    m_map[key] = value.to_str();
  }

  return true;
}
}  // namespace GraphicsModSystem::Config
