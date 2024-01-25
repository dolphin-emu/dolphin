// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModFeature.h"

#include "Common/Logging/Log.h"

void GraphicsModFeatureConfig::SerializeToConfig(picojson::object& json_obj) const
{
  json_obj.emplace("group", m_group);
  json_obj.emplace("action", m_action);
  json_obj.emplace("action_data", m_action_data);
}

bool GraphicsModFeatureConfig::DeserializeFromConfig(const picojson::object& obj)
{
  if (auto group_iter = obj.find("group"); group_iter != obj.end())
  {
    if (!group_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load mod configuration file, specified feature's group is not a string");
      return false;
    }
    m_group = group_iter->second.get<std::string>();
  }

  if (auto action_iter = obj.find("action"); action_iter != obj.end())
  {
    if (!action_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load mod configuration file, specified feature's action is not a string");
      return false;
    }
    m_action = action_iter->second.get<std::string>();
  }

  if (auto action_data_iter = obj.find("action_data"); action_data_iter != obj.end())
  {
    m_action_data = action_data_iter->second;
  }

  return true;
}

void GraphicsModFeatureConfig::SerializeToProfile(picojson::object*) const
{
}

void GraphicsModFeatureConfig::DeserializeFromProfile(const picojson::object&)
{
}
