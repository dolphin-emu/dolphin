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
#include "InputCommon/ImageOperations.h"
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
  DynamicInputTextures::OutputDetails output;
  for (const auto& configuration : m_configuration)
  {
    configuration.GenerateTextures(file, controller_names, &output);
  }

  const std::string& game_id = SConfig::GetInstance().GetGameID();
  for (const auto& [generated_folder_name, images] : output)
  {
    const auto hi_res_folder = File::GetUserPath(D_HIRESTEXTURES_IDX) + generated_folder_name;

    if (!File::IsDirectory(hi_res_folder))
    {
      File::CreateDir(hi_res_folder);
    }

    const auto game_id_folder = hi_res_folder + DIR_SEP + "gameids";
    if (!File::IsDirectory(game_id_folder))
    {
      File::CreateDir(game_id_folder);
    }

    File::CreateEmptyFile(game_id_folder + DIR_SEP + game_id + ".txt");

    for (const auto& [image_name, image] : images)
    {
      WriteImage(hi_res_folder + DIR_SEP + image_name, image);
    }
  }
}
}  // namespace InputCommon
