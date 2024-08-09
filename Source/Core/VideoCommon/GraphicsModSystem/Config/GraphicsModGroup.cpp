// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModGroup.h"

#include <map>
#include <sstream>
#include <string>

#include <picojson.h>
#include <xxh3.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"
#include "Core/ConfigManager.h"

#include "VideoCommon/GraphicsModSystem/Constants.h"
#include "VideoCommon/HiresTextures.h"

namespace GraphicsModSystem::Config
{
GraphicsModGroup::GraphicsModGroup(std::string game_id) : m_game_id(std::move(game_id))
{
}

void GraphicsModGroup::Load()
{
  struct GraphicsModWithDir
  {
    Config::GraphicsMod m_mod;
    std::string m_path;
  };
  std::vector<GraphicsModWithDir> action_only_mods;
  std::vector<GraphicsModWithDir> target_only_mods;
  const auto try_add_mod = [&](const std::string& dir) {
    auto file = dir + DIR_SEP + "metadata.json";
    UnifyPathSeparators(file);

    if (auto mod = GraphicsMod::Create(file))
    {
      if (mod->m_actions.empty() && mod->m_targets.empty())
        return;

      // Actions can be empty for mods that just define
      // targets for use by other mods
      // These are mainly used by Dolphin default mods
      if (mod->m_actions.empty())
      {
        GraphicsModWithDir mod_with_dir;
        mod_with_dir.m_mod = std::move(*mod);
        mod_with_dir.m_path = dir;
        target_only_mods.push_back(std::move(mod_with_dir));
        return;
      }

      // Targets can be empty for mods that define some actions
      // and a tag
      if (mod->m_targets.empty())
      {
        GraphicsModWithDir mod_with_dir;
        mod_with_dir.m_mod = std::move(*mod);
        mod_with_dir.m_path = dir;
        action_only_mods.push_back(std::move(mod_with_dir));
        return;
      }

      std::string file_data;
      if (!File::ReadFileToString(file, file_data))
        return;
      GraphicsModGroup::GraphicsModWithMetadata mod_with_metadata;
      mod_with_metadata.m_path = dir;
      mod_with_metadata.m_mod = std::move(*mod);
      mod_with_metadata.m_id = XXH3_64bits(file_data.data(), file_data.size());
      m_graphics_mods.push_back(std::move(mod_with_metadata));
    }
  };

  const std::set<std::string> graphics_mod_user_directories =
      GetTextureDirectoriesWithGameId(File::GetUserPath(D_GRAPHICSMOD_IDX), m_game_id);

  for (const auto& graphics_mod_directory : graphics_mod_user_directories)
  {
    try_add_mod(graphics_mod_directory);
  }

  const std::set<std::string> graphics_mod_system_directories = GetTextureDirectoriesWithGameId(
      File::GetSysDirectory() + DOLPHIN_SYSTEM_GRAPHICS_MOD_DIR, m_game_id);

  for (const auto& graphics_mod_directory : graphics_mod_system_directories)
  {
    try_add_mod(graphics_mod_directory);
  }

  // Now build some mods that are combination of multiple mods
  // (typically used by Dolphin built-in mods)
  for (const auto& action_only_mod_with_dir : action_only_mods)
  {
    // The way the action only mods interact with the target only mods is through tags
    // If there are no tags, then no reason to care about this mod
    if (action_only_mod_with_dir.m_mod.m_actions.empty() ||
        action_only_mod_with_dir.m_mod.m_tag_name_to_action_indexes.empty())
    {
      continue;
    }

    XXH3_state_t id_hash;
    XXH3_INITSTATE(&id_hash);
    XXH3_64bits_reset_withSeed(&id_hash, static_cast<XXH64_hash_t>(1));

    const auto action_only_file = action_only_mod_with_dir.m_path + DIR_SEP + "metadata.json";
    std::string action_only_file_data;
    if (!File::ReadFileToString(action_only_file, action_only_file_data))
      continue;
    XXH3_64bits_update(&id_hash, action_only_file_data.data(), action_only_file_data.size());

    GraphicsMod combined_mod = action_only_mod_with_dir.m_mod;

    for (const auto& target_only_mod_with_dir : target_only_mods)
    {
      bool target_found = false;
      for (const auto& target : target_only_mod_with_dir.m_mod.m_targets)
      {
        const auto contains_tag_name = [&](const GenericTarget& underlying_target) {
          return std::any_of(
              underlying_target.m_tag_names.begin(), underlying_target.m_tag_names.end(),
              [&](const auto& tag_name) {
                return action_only_mod_with_dir.m_mod.m_tag_name_to_action_indexes.contains(
                    tag_name);
              });
        };

        // Only be interested in this mod's target if the tag
        // exists in the action mod's tag list
        std::visit(overloaded{[&](const Config::IntTarget& int_target) {
                                if (contains_tag_name(int_target))
                                {
                                  combined_mod.m_targets.push_back(target);
                                  target_found = true;
                                }
                              },
                              [&](const Config::StringTarget& str_target) {
                                if (contains_tag_name(str_target))
                                {
                                  combined_mod.m_targets.push_back(target);
                                  target_found = true;
                                }
                              }},
                   target);
      }

      if (target_found)
      {
        const auto file = target_only_mod_with_dir.m_path + DIR_SEP + "metadata.json";
        std::string target_file_data;
        if (!File::ReadFileToString(file, target_file_data))
          continue;
        XXH3_64bits_update(&id_hash, target_file_data.data(), target_file_data.size());
      }
    }

    GraphicsModGroup::GraphicsModWithMetadata mod_with_metadata;
    mod_with_metadata.m_path = action_only_mod_with_dir.m_path;
    mod_with_metadata.m_mod = std::move(combined_mod);
    mod_with_metadata.m_id = XXH3_64bits_digest(&id_hash);
    m_graphics_mods.push_back(std::move(mod_with_metadata));
  }

  for (auto& mod : m_graphics_mods)
  {
    m_id_to_graphics_mod[mod.m_id] = &mod;
  }

  const auto gameid_metadata = GetPath();
  if (File::Exists(gameid_metadata))
  {
    picojson::value root;
    std::string error;
    if (!JsonFromFile(gameid_metadata, &root, &error))
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load graphics mod group json file '{}' due to parse error: {}",
                    gameid_metadata, error);
      return;
    }

    if (!root.is<picojson::object>())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load graphics mod group json file '{}' due to root not being an object!",
          gameid_metadata);
      return;
    }

    const auto& mods = root.get("mods");
    if (mods.is<picojson::array>())
    {
      for (const auto& mod_json : mods.get<picojson::array>())
      {
        if (mod_json.is<picojson::object>())
        {
          const auto& mod_json_obj = mod_json.get<picojson::object>();
          const auto id_str = ReadStringFromJson(mod_json_obj, "id");
          if (!id_str)
            continue;
          u64 id;
          if (!TryParse(*id_str, &id))
          {
            continue;
          }
          if (const auto iter = m_id_to_graphics_mod.find(id); iter != m_id_to_graphics_mod.end())
          {
            iter->second->m_weight = ReadNumericFromJson<u16>(mod_json_obj, "weight").value_or(0);
            iter->second->m_enabled = ReadBoolFromJson(mod_json_obj, "enabled").value_or(false);
          }
        }
      }
    }
  }

  std::sort(m_graphics_mods.begin(), m_graphics_mods.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.m_weight < rhs.m_weight; });

  m_change_count++;
}

void GraphicsModGroup::Save() const
{
  picojson::object serialized_root;
  picojson::array serialized_mods;
  for (const auto& [id, mod_ptr] : m_id_to_graphics_mod)
  {
    picojson::object serialized_mod;
    serialized_mod.emplace("id", std::to_string(id));
    serialized_mod.emplace("enabled", mod_ptr->m_enabled);
    serialized_mod.emplace("weight", static_cast<double>(mod_ptr->m_weight));
    serialized_mods.emplace_back(std::move(serialized_mod));
  }
  serialized_root.emplace("mods", std::move(serialized_mods));

  const auto file_path = GetPath();
  if (!JsonToFile(file_path, picojson::value{serialized_root}, true))
  {
    ERROR_LOG_FMT(VIDEO, "Failed to open graphics mod group json file '{}' for writing", file_path);
  }
}

void GraphicsModGroup::SetChangeCount(u32 change_count)
{
  m_change_count = change_count;
}

u32 GraphicsModGroup::GetChangeCount() const
{
  return m_change_count;
}

const std::vector<GraphicsModGroup::GraphicsModWithMetadata>& GraphicsModGroup::GetMods() const
{
  return m_graphics_mods;
}

std::vector<GraphicsModGroup::GraphicsModWithMetadata>& GraphicsModGroup::GetMods()
{
  return m_graphics_mods;
}

GraphicsModGroup::GraphicsModWithMetadata* GraphicsModGroup::GetMod(u64 id) const
{
  if (const auto iter = m_id_to_graphics_mod.find(id); iter != m_id_to_graphics_mod.end())
  {
    return iter->second;
  }

  return nullptr;
}

const std::string& GraphicsModGroup::GetGameID() const
{
  return m_game_id;
}

std::string GraphicsModGroup::GetPath() const
{
  const std::string game_mod_root = File::GetUserPath(D_CONFIG_IDX) + GRAPHICSMOD_CONFIG_DIR;
  return fmt::format("{}/{}.json", game_mod_root, m_game_id);
}
}  // namespace GraphicsModSystem::Config
