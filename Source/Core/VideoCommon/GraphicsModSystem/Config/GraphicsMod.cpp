// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "VideoCommon/GraphicsModSystem/Constants.h"

namespace GraphicsModSystem::Config
{
std::optional<GraphicsMod> GraphicsMod::Create(const std::string& file_path)
{
  picojson::value root;
  std::string error;
  if (!JsonFromFile(file_path, &root, &error))
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load graphics mod json file '{}' due to parse error: {}",
                  file_path, error);
    return std::nullopt;
  }

  GraphicsMod result;
  if (!result.Deserialize(root))
  {
    return std::nullopt;
  }
  return result;
}

void GraphicsMod::Serialize(picojson::object& json_obj) const
{
  picojson::object serialized_metadata;
  serialized_metadata.emplace("schema_version", static_cast<double>(m_schema_version));
  serialized_metadata.emplace("title", m_title);
  serialized_metadata.emplace("author", m_author);
  serialized_metadata.emplace("description", m_description);
  serialized_metadata.emplace("mod_version", m_mod_version);
  json_obj.emplace("meta", std::move(serialized_metadata));

  picojson::array serialized_assets;
  for (const auto& asset : m_assets)
  {
    picojson::object serialized_asset;
    asset.Serialize(serialized_asset);
    serialized_assets.emplace_back(std::move(serialized_asset));
  }
  json_obj.emplace("assets", std::move(serialized_assets));

  picojson::array serialized_tags;
  for (const auto& tag : m_tags)
  {
    picojson::object serialized_tag;
    tag.Serialize(serialized_tag);
    serialized_tags.emplace_back(std::move(serialized_tag));
  }
  json_obj.emplace("tags", std::move(serialized_tags));

  picojson::array serialized_targets;
  for (const auto& target : m_targets)
  {
    picojson::object serialized_target;
    SerializeTarget(serialized_target, target);
    serialized_targets.emplace_back(std::move(serialized_target));
  }
  json_obj.emplace("targets", std::move(serialized_targets));

  picojson::array serialized_actions;
  for (const auto& action : m_actions)
  {
    picojson::object serialized_action;
    action.Serialize(serialized_action);
    serialized_actions.emplace_back(std::move(serialized_action));
  }
  json_obj.emplace("actions", std::move(serialized_actions));

  picojson::object serialized_target_to_actions;
  for (const auto& [target_index, action_indexes] : m_target_index_to_action_indexes)
  {
    picojson::array serialized_action_indexes;
    for (const auto& action_index : action_indexes)
    {
      serialized_action_indexes.emplace_back(static_cast<double>(action_index));
    }
    serialized_target_to_actions.emplace(std::to_string(target_index),
                                         std::move(serialized_action_indexes));
  }
  json_obj.emplace("target_to_actions", serialized_target_to_actions);

  picojson::object serialized_tag_to_actions;
  for (const auto& [tag_name, action_indexes] : m_tag_name_to_action_indexes)
  {
    picojson::array serialized_action_indexes;
    for (const auto& action_index : action_indexes)
    {
      serialized_action_indexes.emplace_back(static_cast<double>(action_index));
    }
    serialized_tag_to_actions.emplace(tag_name, std::move(serialized_action_indexes));
  }
  json_obj.emplace("tag_to_actions", serialized_tag_to_actions);

  picojson::object serialized_hash_policy;
  serialized_hash_policy.emplace("attributes",
                                 HashAttributesToString(m_default_hash_policy.attributes));
  json_obj.emplace("default_hash_policy", serialized_hash_policy);
}

bool GraphicsMod::Deserialize(const picojson::value& value)
{
  const auto& meta = value.get("meta");
  if (meta.is<picojson::object>())
  {
    const auto& meta_obj = meta.get<picojson::object>();
    m_schema_version = ReadNumericFromJson<u16>(meta_obj, "schema_version").value_or(0);
    if (m_schema_version != LATEST_SCHEMA_VERSION)
    {
      // For now error, we can handle schema migrations in the future?
      ERROR_LOG_FMT(VIDEO,
                    "Failed to deserialize graphics mod data, schema_version was '{}' but latest "
                    "version is '{}'",
                    m_schema_version, LATEST_SCHEMA_VERSION);
      return false;
    }

    m_title = ReadStringFromJson(meta_obj, "title").value_or("Unknown Mod");
    m_author = ReadStringFromJson(meta_obj, "author").value_or("Unknown");
    m_description = ReadStringFromJson(meta_obj, "description").value_or("");
    m_mod_version = ReadStringFromJson(meta_obj, "mod_version").value_or("v0.0.0");
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
      GraphicsModAsset asset;
      if (!asset.Deserialize(asset_val.get<picojson::object>()))
      {
        return false;
      }

      m_assets.push_back(std::move(asset));
    }
  }

  const auto& tags = value.get("tags");
  if (tags.is<picojson::array>())
  {
    for (const auto& tag_val : tags.get<picojson::array>())
    {
      if (!tag_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load mod configuration file, specified tag is not a json object");
        return false;
      }
      GraphicsModTag tag;
      if (!tag.Deserialize(tag_val.get<picojson::object>()))
      {
        return false;
      }

      m_tags.push_back(std::move(tag));
    }
  }

  const auto& targets = value.get("targets");
  if (targets.is<picojson::array>())
  {
    for (const auto& target_val : targets.get<picojson::array>())
    {
      if (!target_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(
            VIDEO, "Failed to load mod configuration file, specified target is not a json object");
        return false;
      }

      AnyTarget target;
      if (!DeserializeTarget(target_val.get<picojson::object>(), target))
        return false;

      m_targets.push_back(std::move(target));
    }
  }

  const auto& actions = value.get("actions");
  if (actions.is<picojson::array>())
  {
    for (const auto& action_val : actions.get<picojson::array>())
    {
      if (!action_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(
            VIDEO, "Failed to load mod configuration file, specified action is not a json object");
        return false;
      }
      GraphicsModAction action;
      if (!action.Deserialize(action_val.get<picojson::object>()))
      {
        return false;
      }

      m_actions.push_back(std::move(action));
    }
  }

  const auto& target_to_actions = value.get("target_to_actions");
  if (target_to_actions.is<picojson::object>())
  {
    for (const auto& [key, action_indexes_val] : target_to_actions.get<picojson::object>())
    {
      u64 target_index = 0;
      if (!TryParse(key, &target_index))
      {
        ERROR_LOG_FMT(
            VIDEO, "Failed to load mod configuration file, specified target index is not a number");
        return false;
      }

      auto& action_indexes = m_target_index_to_action_indexes[target_index];
      if (!action_indexes_val.is<picojson::array>())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load mod configuration file, specified target index '{}' has "
                      "a non-array action index value",
                      target_index);
      }

      for (const auto& action_index_val : action_indexes_val.get<picojson::array>())
      {
        if (!action_index_val.is<double>())
        {
          ERROR_LOG_FMT(VIDEO,
                        "Failed to load mod configuration file, specified target index '{}' has "
                        "a non numeric action index",
                        target_index);
          return false;
        }
        action_indexes.push_back(static_cast<u64>(action_index_val.get<double>()));
      }
    }
  }

  const auto& tag_to_actions = value.get("tag_to_actions");
  if (tag_to_actions.is<picojson::object>())
  {
    for (const auto& [tag, action_indexes_val] : tag_to_actions.get<picojson::object>())
    {
      auto& action_indexes = m_tag_name_to_action_indexes[tag];
      if (!action_indexes_val.is<picojson::array>())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load mod configuration file, specified tag '{}' has "
                      "a non-array action index value",
                      tag);
      }

      for (const auto& action_index_val : action_indexes_val.get<picojson::array>())
      {
        if (!action_index_val.is<double>())
        {
          ERROR_LOG_FMT(VIDEO,
                        "Failed to load mod configuration file, specified tag '{}' has "
                        "a non numeric action index",
                        tag);
          return false;
        }
        action_indexes.push_back(static_cast<u64>(action_index_val.get<double>()));
      }
    }
  }

  m_default_hash_policy = GetDefaultHashPolicy();

  const auto& default_hash_policy_json = value.get("default_hash_policy");
  if (default_hash_policy_json.is<picojson::object>())
  {
    auto& default_hash_policy_json_obj = default_hash_policy_json.get<picojson::object>();
    const auto attributes = ReadStringFromJson(default_hash_policy_json_obj, "attributes");
    if (attributes)
    {
      m_default_hash_policy.attributes = HashAttributesFromString(*attributes);
    }
  }

  return true;
}
}  // namespace GraphicsModSystem::Config
