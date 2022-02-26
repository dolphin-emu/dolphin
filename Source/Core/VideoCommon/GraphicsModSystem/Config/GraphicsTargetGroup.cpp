// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsTargetGroup.h"

#include "Common/Logging/Log.h"

bool GraphicsTargetGroupConfig::DeserializeFromConfig(const picojson::object& obj)
{
  if (auto name_iter = obj.find("name"); name_iter != obj.end())
  {
    if (!name_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(
          VIDEO, "Failed to load mod configuration file, specified group's name is not a string");
      return false;
    }
    m_name = name_iter->second.get<std::string>();
  }

  if (auto targets_iter = obj.find("targets"); targets_iter != obj.end())
  {
    if (!targets_iter->second.is<picojson::array>())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load mod configuration file, specified group's targets is not an array");
      return false;
    }
    for (const auto& target_val : targets_iter->second.get<picojson::array>())
    {
      if (!target_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(
            VIDEO,
            "Failed to load shader configuration file, specified target is not a json object");
        return false;
      }
      const auto target = DeserializeTargetFromConfig(target_val.get<picojson::object>());
      if (!target)
      {
        return false;
      }

      m_targets.push_back(*target);
    }
  }

  return true;
}

void GraphicsTargetGroupConfig::SerializeToProfile(picojson::object* obj) const
{
  if (!obj)
    return;
  auto& json_obj = *obj;
  picojson::array serialized_targets;
  for (const auto& target : m_targets)
  {
    picojson::object serialized_target;
    SerializeTargetToProfile(&serialized_target, target);
    serialized_targets.push_back(picojson::value{serialized_target});
  }
  json_obj["targets"] = picojson::value{serialized_targets};
}

void GraphicsTargetGroupConfig::DeserializeFromProfile(const picojson::object& obj)
{
  if (const auto it = obj.find("targets"); it != obj.end())
  {
    if (it->second.is<picojson::array>())
    {
      auto serialized_targets = it->second.get<picojson::array>();
      if (serialized_targets.size() != m_targets.size())
        return;

      for (std::size_t i = 0; i < serialized_targets.size(); i++)
      {
        const auto& serialized_target_val = serialized_targets[i];
        if (serialized_target_val.is<picojson::object>())
        {
          const auto& serialized_target = serialized_target_val.get<picojson::object>();
          DeserializeTargetFromProfile(serialized_target, &m_targets[i]);
        }
      }
    }
  }
}
