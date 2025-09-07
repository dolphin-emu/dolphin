// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "VideoCommon/GraphicsModSystem/Constants.h"

std::optional<GraphicsModConfig> GraphicsModConfig::Create(const std::string& file_path,
                                                           Source source)
{
  nlohmann::json root;
  std::string error;
  if (!JsonFromFile(file_path, &root, &error))
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

std::optional<GraphicsModConfig> GraphicsModConfig::Create(const nlohmann::json* obj)
{
  if (!obj)
    return std::nullopt;

  const std::optional<std::string> source_str = ReadStringFromJson(*obj, "source");
  if (!source_str)
    return std::nullopt;

  const std::optional<std::string> relative_path = ReadStringFromJson(*obj, "path");
  if (!relative_path)
    return std::nullopt;

  if (*source_str == "system")
  {
    return Create(fmt::format("{}{}{}", File::GetSysDirectory(), DOLPHIN_SYSTEM_GRAPHICS_MOD_DIR,
                              *relative_path),
                  Source::System);
  }
  else
  {
    return Create(File::GetUserPath(D_GRAPHICSMOD_IDX) + *relative_path, Source::User);
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

void GraphicsModConfig::SerializeToConfig(nlohmann::json& json_obj) const
{
  nlohmann::json serialized_metadata;
  serialized_metadata["title"] = m_title;
  serialized_metadata["author"] = m_author;
  serialized_metadata["description"] = m_description;
  json_obj["meta"] = std::move(serialized_metadata);

  nlohmann::json serialized_groups;
  for (const auto& group : m_groups)
  {
    nlohmann::json serialized_group;
    group.SerializeToConfig(serialized_group);
    serialized_groups.push_back(std::move(serialized_group));
  }
  json_obj["groups"] = std::move(serialized_groups);

  nlohmann::json serialized_features;
  for (const auto& feature : m_features)
  {
    nlohmann::json serialized_feature;
    feature.SerializeToConfig(serialized_feature);
    serialized_features.push_back(std::move(serialized_feature));
  }
  json_obj["features"] = std::move(serialized_features);

  nlohmann::json serialized_assets;
  for (const auto& asset : m_assets)
  {
    nlohmann::json serialized_asset;
    asset.SerializeToConfig(serialized_asset);
    serialized_assets.push_back(std::move(serialized_asset));
  }
  json_obj["assets"] = std::move(serialized_assets);
}

bool GraphicsModConfig::DeserializeFromConfig(const nlohmann::json& value)
{
  if (const auto& meta = ReadObjectFromJson(value, "meta"))
  {
    m_title = ReadStringFromJson(meta, "title").value_or(m_title);
    m_author = ReadStringFromJson(meta, "author").value_or(m_author);
    m_description = ReadStringFromJson(meta, "description").value_or(m_description);
  }

  if (const auto& groups = ReadArrayFromJson(value, "groups"))
  {
    for (const auto& group_val : *groups)
    {
      if (!group_val.is_object())
      {
        ERROR_LOG_FMT(
            VIDEO, "Failed to load mod configuration file, specified group is not a json object");
        return false;
      }
      GraphicsTargetGroupConfig group;
      if (!group.DeserializeFromConfig(group_val))
      {
        return false;
      }

      m_groups.push_back(std::move(group));
    }
  }

  if (const auto& features = ReadArrayFromJson(value, "features"))
  {
    for (const auto& feature_val : *features)
    {
      if (!feature_val.is_object())
      {
        ERROR_LOG_FMT(
            VIDEO, "Failed to load mod configuration file, specified feature is not a json object");
        return false;
      }
      GraphicsModFeatureConfig feature;
      if (!feature.DeserializeFromConfig(feature_val))
      {
        return false;
      }

      m_features.push_back(std::move(feature));
    }
  }

  if (const auto& assets = ReadArrayFromJson(value, "assets"))
  {
    for (const auto& asset_val : *assets)
    {
      if (!asset_val.is_object())
      {
        ERROR_LOG_FMT(
            VIDEO, "Failed to load mod configuration file, specified asset is not a json object");
        return false;
      }
      GraphicsModAssetConfig asset;
      if (!asset.DeserializeFromConfig(asset_val))
      {
        return false;
      }

      m_assets.push_back(std::move(asset));
    }
  }

  return true;
}

void GraphicsModConfig::SerializeToProfile(nlohmann::json* obj) const
{
  if (!obj)
    return;

  auto& json_obj = *obj;
  switch (m_source)
  {
  case Source::User:
    json_obj["source"] = "user";
    break;
  case Source::System:
    json_obj["source"] = "system";
    break;
  }

  json_obj["path"] = m_relative_path;

  nlohmann::json serialized_groups;
  for (const auto& group : m_groups)
  {
    nlohmann::json serialized_group;
    group.SerializeToProfile(&serialized_group);
    serialized_groups.push_back(std::move(serialized_group));
  }
  json_obj["groups"] = std::move(serialized_groups);

  nlohmann::json serialized_features;
  for (const auto& feature : m_features)
  {
    nlohmann::json serialized_feature;
    feature.SerializeToProfile(&serialized_feature);
    serialized_features.push_back(std::move(serialized_feature));
  }
  json_obj["features"] = std::move(serialized_features);

  json_obj["enabled"] = m_enabled;

  json_obj["weight"] = m_weight;
}

void GraphicsModConfig::DeserializeFromProfile(const nlohmann::json& obj)
{
  if (const auto& serialized_groups = ReadArrayFromJson(obj, "groups"))
  {
    if (serialized_groups->size() != m_groups.size())
      return;

    for (std::size_t i = 0; i < serialized_groups->size(); i++)
    {
      if (serialized_groups->at(i).is_object())
        m_groups[i].DeserializeFromProfile(serialized_groups->at(i));
    }
  }

  if (const auto& serialized_features = ReadArrayFromJson(obj, "features"))
  {
    if (serialized_features->size() != m_features.size())
      return;

    for (std::size_t i = 0; i < serialized_features->size(); i++)
    {
      if (serialized_features->at(i).is_object())
        m_features[i].DeserializeFromProfile(serialized_features->at(i));
    }
  }

  m_enabled = ReadBoolFromJson(obj, "enabled").value_or(m_enabled);
  m_weight = ReadNumericFromJson<u16>(obj, "weight").value_or(m_weight);
}
