// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModFeature.h"

#include <nlohmann/json.hpp>

#include "Common/Logging/Log.h"

void GraphicsModFeatureConfig::SerializeToConfig(nlohmann::json& json_obj) const
{
  json_obj["group"] = m_group;
  json_obj["action"] = m_action;
  json_obj["action_data"] = m_action_data;
}

bool GraphicsModFeatureConfig::DeserializeFromConfig(const nlohmann::json& obj)
{
  if (auto group_it = obj.find("group"); group_it != obj.end())
  {
    if (!group_it->is_string())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load mod configuration file, specified feature's group is not a string");
      return false;
    }
    m_group = group_it->get<std::string>();
  }

  if (auto action_it = obj.find("action"); action_it != obj.end())
  {
    if (!action_it->is_string())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load mod configuration file, specified feature's action is not a string");
      return false;
    }
    m_action = action_it->get<std::string>();
  }

  if (auto action_data_it = obj.find("action_data"); action_data_it != obj.end())
    m_action_data = *action_data_it;

  return true;
}

void GraphicsModFeatureConfig::SerializeToProfile(nlohmann::json*) const
{
}

void GraphicsModFeatureConfig::DeserializeFromProfile(const nlohmann::json&)
{
}
