// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsTargetGroup.h"

#include <nlohmann/json.hpp>

#include "Common/Logging/Log.h"

void GraphicsTargetGroupConfig::SerializeToConfig(nlohmann::json& json_obj) const
{
  nlohmann::json serialized_targets;
  for (const auto& target : m_targets)
  {
    nlohmann::json serialized_target;
    SerializeTargetToConfig(serialized_target, target);
    serialized_targets.push_back(std::move(serialized_target));
  }
  json_obj["targets"] = std::move(serialized_targets);
  json_obj["name"] = m_name;
}

bool GraphicsTargetGroupConfig::DeserializeFromConfig(const nlohmann::json& obj)
{
  if (auto name_it = obj.find("name"); name_it != obj.end())
  {
    if (!name_it->is_string())
    {
      ERROR_LOG_FMT(
          VIDEO, "Failed to load mod configuration file, specified group's name is not a string");
      return false;
    }
    m_name = name_it->get<std::string>();
  }

  if (auto targets_it = obj.find("targets"); targets_it != obj.end())
  {
    const auto& targets = *targets_it;
    if (!targets.is_array())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load mod configuration file, specified group's targets is not an array");
      return false;
    }
    for (const auto& target_val : targets)
    {
      if (!target_val.is_object())
      {
        ERROR_LOG_FMT(
            VIDEO,
            "Failed to load shader configuration file, specified target is not a json object");
        return false;
      }
      const auto target = DeserializeTargetFromConfig(target_val);
      if (!target)
      {
        return false;
      }

      m_targets.push_back(*target);
    }
  }

  return true;
}

void GraphicsTargetGroupConfig::SerializeToProfile(nlohmann::json* obj) const
{
  if (!obj)
    return;
  auto& json_obj = *obj;
  nlohmann::json serialized_targets;
  for (const auto& target : m_targets)
  {
    nlohmann::json serialized_target;
    SerializeTargetToProfile(&serialized_target, target);
    serialized_targets.push_back(std::move(serialized_target));
  }
  json_obj["targets"] = std::move(serialized_targets);
}

void GraphicsTargetGroupConfig::DeserializeFromProfile(const nlohmann::json& obj)
{
  if (!obj.is_object())
    return;

  auto it = obj.find("targets");
  if (it == obj.end())
    return;

  const auto& serialized_targets = *it;
  if (!serialized_targets.is_array() || serialized_targets.size() != m_targets.size())
    return;

  for (std::size_t i = 0; i < serialized_targets.size(); i++)
  {
    const auto& serialized_target_val = serialized_targets[i];
    if (serialized_target_val.is_object())
    {
      const auto& serialized_target = serialized_target_val;
      DeserializeTargetFromProfile(serialized_target, &m_targets[i]);
    }
  }
}
