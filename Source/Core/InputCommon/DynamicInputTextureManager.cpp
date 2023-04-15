// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/DynamicInputTextureManager.h"

#include <set>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "InputCommon/DynamicInputTextures/DITConfiguration.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/TextureCacheBase.h"

namespace InputCommon
{
DynamicInputTextureManager::DynamicInputTextureManager() = default;

DynamicInputTextureManager::~DynamicInputTextureManager() = default;

void DynamicInputTextureManager::Load()
{
  m_configuration.clear();

  const std::string& game_id = SConfig::GetInstance().GetGameID();
  const std::set<std::string> dynamic_input_directories =
      GetTextureDirectoriesWithGameId(File::GetUserPath(D_DYNAMICINPUT_IDX), game_id);

  for (const auto& dynamic_input_directory : dynamic_input_directories)
  {
    const auto json_files = Common::DoFileSearch({dynamic_input_directory}, {".json"});
    for (auto& file : json_files)
    {
      m_configuration.emplace_back(file);
    }
  }
}

void DynamicInputTextureManager::GenerateTextures(const Common::IniFile& file,
                                                  const std::vector<std::string>& controller_names)
{
  bool any_dirty = false;
  for (const auto& configuration : m_configuration)
  {
    any_dirty |= configuration.GenerateTextures(file, controller_names);
  }

  if (any_dirty && g_texture_cache && Core::GetState() != Core::State::Starting)
    g_texture_cache->ForceReloadTextures();
}
}  // namespace InputCommon
