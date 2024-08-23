// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModGroup.h"

#include <map>
#include <sstream>
#include <string>

#include <picojson.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"

#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"
#include "VideoCommon/GraphicsModSystem/Constants.h"
#include "VideoCommon/HiresTextures.h"

GraphicsModGroupConfig::GraphicsModGroupConfig(std::string game_id) : m_game_id(std::move(game_id))
{
}

GraphicsModGroupConfig::~GraphicsModGroupConfig() = default;

GraphicsModGroupConfig::GraphicsModGroupConfig(const GraphicsModGroupConfig&) = default;

GraphicsModGroupConfig::GraphicsModGroupConfig(GraphicsModGroupConfig&&) noexcept = default;

GraphicsModGroupConfig& GraphicsModGroupConfig::operator=(const GraphicsModGroupConfig&) = default;

GraphicsModGroupConfig&
GraphicsModGroupConfig::operator=(GraphicsModGroupConfig&&) noexcept = default;

void GraphicsModGroupConfig::Load()
{
  const std::string file_path = GetPath();

  std::set<std::string> known_paths;
  if (File::Exists(file_path))
  {
    picojson::value root;
    std::string error;
    if (!JsonFromFile(file_path, &root, &error))
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load graphics mod group json file '{}' due to parse error: {}",
                    file_path, error);
      return;
    }
    if (!root.is<picojson::object>())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load graphics mod group json file '{}' due to root not being an object!",
          file_path);
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
          auto graphics_mod = GraphicsModConfig::Create(&mod_json_obj);
          if (!graphics_mod)
          {
            continue;
          }
          graphics_mod->DeserializeFromProfile(mod_json_obj);

          auto mod_full_path = graphics_mod->GetAbsolutePath();
          known_paths.insert(std::move(mod_full_path));
          m_graphics_mods.push_back(std::move(*graphics_mod));
        }
      }
    }
  }

  const auto try_add_mod = [&known_paths, this](const std::string& dir,
                                                GraphicsModConfig::Source source) {
    auto file = dir + DIR_SEP + "metadata.json";
    UnifyPathSeparators(file);
    if (known_paths.contains(file))
      return;

    if (auto mod = GraphicsModConfig::Create(file, source))
      m_graphics_mods.push_back(std::move(*mod));
  };

  const std::set<std::string> graphics_mod_user_directories =
      GetTextureDirectoriesWithGameId(File::GetUserPath(D_GRAPHICSMOD_IDX), m_game_id);

  for (const auto& graphics_mod_directory : graphics_mod_user_directories)
  {
    try_add_mod(graphics_mod_directory, GraphicsModConfig::Source::User);
  }

  const std::set<std::string> graphics_mod_system_directories = GetTextureDirectoriesWithGameId(
      File::GetSysDirectory() + DOLPHIN_SYSTEM_GRAPHICS_MOD_DIR, m_game_id);

  for (const auto& graphics_mod_directory : graphics_mod_system_directories)
  {
    try_add_mod(graphics_mod_directory, GraphicsModConfig::Source::System);
  }

  std::sort(m_graphics_mods.begin(), m_graphics_mods.end());
  for (auto& mod : m_graphics_mods)
  {
    m_path_to_graphics_mod[mod.GetAbsolutePath()] = &mod;
  }

  m_change_count++;
}

void GraphicsModGroupConfig::Save() const
{
  const std::string file_path = GetPath();
  std::ofstream json_stream;
  File::OpenFStream(json_stream, file_path, std::ios_base::out);
  if (!json_stream.is_open())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to open graphics mod group json file '{}' for writing", file_path);
    return;
  }

  picojson::object serialized_root;
  picojson::array serialized_mods;
  for (const auto& mod : m_graphics_mods)
  {
    picojson::object serialized_mod;
    mod.SerializeToProfile(&serialized_mod);
    serialized_mods.emplace_back(std::move(serialized_mod));
  }
  serialized_root.emplace("mods", std::move(serialized_mods));

  const auto output = picojson::value{serialized_root}.serialize(true);
  json_stream << output;
}

void GraphicsModGroupConfig::SetChangeCount(u32 change_count)
{
  m_change_count = change_count;
}

u32 GraphicsModGroupConfig::GetChangeCount() const
{
  return m_change_count;
}

const std::vector<GraphicsModConfig>& GraphicsModGroupConfig::GetMods() const
{
  return m_graphics_mods;
}

std::vector<GraphicsModConfig>& GraphicsModGroupConfig::GetMods()
{
  return m_graphics_mods;
}

GraphicsModConfig* GraphicsModGroupConfig::GetMod(std::string_view absolute_path) const
{
  if (const auto iter = m_path_to_graphics_mod.find(absolute_path);
      iter != m_path_to_graphics_mod.end())
  {
    return iter->second;
  }

  return nullptr;
}

const std::string& GraphicsModGroupConfig::GetGameID() const
{
  return m_game_id;
}

std::string GraphicsModGroupConfig::GetPath() const
{
  const std::string game_mod_root = File::GetUserPath(D_CONFIG_IDX) + GRAPHICSMOD_CONFIG_DIR;
  return fmt::format("{}/{}.json", game_mod_root, m_game_id);
}
