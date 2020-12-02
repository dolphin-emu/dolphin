// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/DynamicInputTextureManager.h"

#include <set>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"

#include "InputCommon/DynamicInputTextureConfiguration.h"
#include "VideoCommon/HiresTextures.h"

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

void DynamicInputTextureManager::GenerateTextures(const IniFile::Section* sec,
                                                  const std::string& controller_name)
{
  for (const auto& configuration : m_configuration)
  {
    configuration.GenerateTextures(sec, controller_name);
  }
}
}  // namespace InputCommon
