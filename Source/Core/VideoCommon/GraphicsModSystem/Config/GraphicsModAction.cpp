// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModAction.h"

#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"

namespace GraphicsModSystem::Config
{
void GraphicsModAction::Serialize(picojson::object& json_obj) const
{
  json_obj.emplace("factory_name", m_factory_name);
  json_obj.emplace("data", m_data);
}

bool GraphicsModAction::Deserialize(const picojson::object& obj)
{
  const auto factory_name = ReadStringFromJson(obj, "factory_name");
  if (!factory_name)
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Failed to load mod configuration file, specified action's factory_name is not valid");
    return false;
  }
  m_factory_name = *factory_name;

  if (auto data_iter = obj.find("data"); data_iter != obj.end())
  {
    m_data = data_iter->second;
  }

  return true;
}
}  // namespace GraphicsModSystem::Config
