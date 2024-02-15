// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "VideoCommon/GraphicsModSystem/Constants.h"

std::optional<GraphicsModConfig> GraphicsModConfig::Create(const std::string& file_path,
                                                           Source source)
{
  std::string json_data;
  if (!File::ReadFileToString(file_path, json_data))
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load graphics mod json file '{}'", file_path);
    return std::nullopt;
  }

  picojson::value root;
  const auto error = picojson::parse(root, json_data);

  if (!error.empty())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load graphics mod json file '{}' due to parse error: {}",
                  file_path, error);
    return std::nullopt;
  }

  GraphicsModConfig result;
  if (!result.DeserializeFromConfig(root))
  {
    return std::nullopt;
  }
  result.m_source = source;
  if (source == Source::User)
  {
    const std::string base_path = File::GetUserPath(D_GRAPHICSMOD_IDX);
    if (base_path.size() > file_path.size())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load graphics mod json file '{}' due to it not matching the base path: {}",
          file_path, base_path);
      return std::nullopt;
    }
    result.m_relative_path = file_path.substr(base_path.size());
  }
  else
  {
    const std::string base_path = File::GetSysDirectory() + DOLPHIN_SYSTEM_GRAPHICS_MOD_DIR;
    if (base_path.size() > file_path.size())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load graphics mod json file '{}' due to it not matching the base path: {}",
          file_path, base_path);
      return std::nullopt;
    }
    result.m_relative_path = file_path.substr(base_path.size());
  }

  return result;
}

std::optional<GraphicsModConfig> GraphicsModConfig::Create(const picojson::object* obj)
{
  if (!obj)
    return std::nullopt;

  const auto source_it = obj->find("source");
  if (source_it == obj->end())
  {
    return std::nullopt;
  }
  const std::string source_str = source_it->second.to_str();

  const auto path_it = obj->find("path");
  if (path_it == obj->end())
  {
    return std::nullopt;
  }
  const std::string relative_path = path_it->second.to_str();

  if (source_str == "system")
  {
    return Create(fmt::format("{}{}{}", File::GetSysDirectory(), DOLPHIN_SYSTEM_GRAPHICS_MOD_DIR,
                              relative_path),
                  Source::System);
  }
  else
  {
    return Create(File::GetUserPath(D_GRAPHICSMOD_IDX) + relative_path, Source::User);
  }
}

std::string GraphicsModConfig::GetAbsolutePath() const
{
  if (m_source == Source::System)
  {
    return WithUnifiedPathSeparators(fmt::format("{}{}{}", File::GetSysDirectory(),
                                                 DOLPHIN_SYSTEM_GRAPHICS_MOD_DIR, m_relative_path));
  }
  else
  {
    return WithUnifiedPathSeparators(File::GetUserPath(D_GRAPHICSMOD_IDX) + m_relative_path);
  }
}

void GraphicsModConfig::SerializeToConfig(picojson::object& json_obj) const
{
  picojson::object serialized_metadata;
  serialized_metadata.emplace("title", m_title);
  serialized_metadata.emplace("author", m_author);
  serialized_metadata.emplace("description", m_description);
  json_obj.emplace("meta", std::move(serialized_metadata));

  picojson::array serialized_groups;
  for (const auto& group : m_groups)
  {
    picojson::object serialized_group;
    group.SerializeToConfig(serialized_group);
    serialized_groups.emplace_back(std::move(serialized_group));
  }
  json_obj.emplace("groups", std::move(serialized_groups));

  picojson::array serialized_features;
  for (const auto& feature : m_features)
  {
    picojson::object serialized_feature;
    feature.SerializeToConfig(serialized_feature);
    serialized_features.emplace_back(std::move(serialized_feature));
  }
  json_obj.emplace("features", std::move(serialized_features));

  picojson::array serialized_assets;
  for (const auto& asset : m_assets)
  {
    picojson::object serialized_asset;
    asset.SerializeToConfig(serialized_asset);
    serialized_assets.emplace_back(std::move(serialized_asset));
  }
  json_obj.emplace("assets", std::move(serialized_assets));
}

bool GraphicsModConfig::DeserializeFromConfig(const picojson::value& value)
{
  const auto& meta = value.get("meta");
  if (meta.is<picojson::object>())
  {
    const auto& title = meta.get("title");
    if (title.is<std::string>())
    {
      m_title = title.to_str();
    }

    const auto& author = meta.get("author");
    if (author.is<std::string>())
    {
      m_author = author.to_str();
    }

    const auto& description = meta.get("description");
    if (description.is<std::string>())
    {
      m_description = description.to_str();
    }
  }

  const auto& groups = value.get("groups");
  if (groups.is<picojson::array>())
  {
    for (const auto& group_val : groups.get<picojson::array>())
    {
      if (!group_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(
            VIDEO, "Failed to load mod configuration file, specified group is not a json object");
        return false;
      }
      GraphicsTargetGroupConfig group;
      if (!group.DeserializeFromConfig(group_val.get<picojson::object>()))
      {
        return false;
      }

      m_groups.push_back(std::move(group));
    }
  }

  const auto& features = value.get("features");
  if (features.is<picojson::array>())
  {
    for (const auto& feature_val : features.get<picojson::array>())
    {
      if (!feature_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(
            VIDEO, "Failed to load mod configuration file, specified feature is not a json object");
        return false;
      }
      GraphicsModFeatureConfig feature;
      if (!feature.DeserializeFromConfig(feature_val.get<picojson::object>()))
      {
        return false;
      }

      m_features.push_back(std::move(feature));
    }
  }

  const auto& assets = value.get("assets");
  if (assets.is<picojson::array>())
  {
    for (const auto& asset_val : assets.get<picojson::array>())
    {
      if (!asset_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(
            VIDEO, "Failed to load mod configuration file, specified asset is not a json object");
        return false;
      }
      GraphicsModAssetConfig asset;
      if (!asset.DeserializeFromConfig(asset_val.get<picojson::object>()))
      {
        return false;
      }

      m_assets.push_back(std::move(asset));
    }
  }

  return true;
}

void GraphicsModConfig::SerializeToProfile(picojson::object* obj) const
{
  if (!obj)
    return;

  auto& json_obj = *obj;
  switch (m_source)
  {
  case Source::User:
    json_obj.emplace("source", "user");
    break;
  case Source::System:
    json_obj.emplace("source", "system");
    break;
  }

  json_obj.emplace("path", m_relative_path);

  picojson::array serialized_groups;
  for (const auto& group : m_groups)
  {
    picojson::object serialized_group;
    group.SerializeToProfile(&serialized_group);
    serialized_groups.emplace_back(std::move(serialized_group));
  }
  json_obj.emplace("groups", std::move(serialized_groups));

  picojson::array serialized_features;
  for (const auto& feature : m_features)
  {
    picojson::object serialized_feature;
    feature.SerializeToProfile(&serialized_feature);
    serialized_features.emplace_back(std::move(serialized_feature));
  }
  json_obj.emplace("features", std::move(serialized_features));

  json_obj.emplace("enabled", m_enabled);

  json_obj.emplace("weight", static_cast<double>(m_weight));
}

void GraphicsModConfig::DeserializeFromProfile(const picojson::object& obj)
{
  if (const auto it = obj.find("groups"); it != obj.end())
  {
    if (it->second.is<picojson::array>())
    {
      const auto& serialized_groups = it->second.get<picojson::array>();
      if (serialized_groups.size() != m_groups.size())
        return;

      for (std::size_t i = 0; i < serialized_groups.size(); i++)
      {
        const auto& serialized_group_val = serialized_groups[i];
        if (serialized_group_val.is<picojson::object>())
        {
          const auto& serialized_group = serialized_group_val.get<picojson::object>();
          m_groups[i].DeserializeFromProfile(serialized_group);
        }
      }
    }
  }

  if (const auto it = obj.find("features"); it != obj.end())
  {
    if (it->second.is<picojson::array>())
    {
      const auto& serialized_features = it->second.get<picojson::array>();
      if (serialized_features.size() != m_features.size())
        return;

      for (std::size_t i = 0; i < serialized_features.size(); i++)
      {
        const auto& serialized_feature_val = serialized_features[i];
        if (serialized_feature_val.is<picojson::object>())
        {
          const auto& serialized_feature = serialized_feature_val.get<picojson::object>();
          m_features[i].DeserializeFromProfile(serialized_feature);
        }
      }
    }
  }

  if (const auto it = obj.find("enabled"); it != obj.end())
  {
    if (it->second.is<bool>())
    {
      m_enabled = it->second.get<bool>();
    }
  }

  if (const auto it = obj.find("weight"); it != obj.end())
  {
    if (it->second.is<double>())
    {
      m_weight = static_cast<u16>(it->second.get<double>());
    }
  }
}

bool GraphicsModConfig::operator<(const GraphicsModConfig& other) const
{
  return m_weight < other.m_weight;
}
