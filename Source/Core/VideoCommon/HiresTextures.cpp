// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/HiresTextures.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <xxhash.h>

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "Core/System.h"
#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

constexpr std::string_view s_format_prefix{"tex1_"};

static std::unordered_map<std::string, std::shared_ptr<HiresTexture>> s_hires_texture_cache;
static std::unordered_map<std::string, bool> s_hires_texture_id_to_arbmipmap;

static auto s_file_library = std::make_shared<VideoCommon::DirectFilesystemAssetLibrary>();

namespace
{
std::pair<std::string, bool> GetNameArbPair(const TextureInfo& texture_info)
{
  if (s_hires_texture_id_to_arbmipmap.empty())
    return {"", false};

  const auto texture_name_details = texture_info.CalculateTextureName();
  // look for an exact match first
  const std::string full_name = texture_name_details.GetFullName();
  if (auto iter = s_hires_texture_id_to_arbmipmap.find(full_name);
      iter != s_hires_texture_id_to_arbmipmap.end())
  {
    return {full_name, iter->second};
  }

  // Single wildcard ignoring the tlut hash
  const std::string texture_name_single_wildcard_tlut =
      fmt::format("{}_{}_$_{}", texture_name_details.base_name, texture_name_details.texture_name,
                  texture_name_details.format_name);
  if (auto iter = s_hires_texture_id_to_arbmipmap.find(texture_name_single_wildcard_tlut);
      iter != s_hires_texture_id_to_arbmipmap.end())
  {
    return {texture_name_single_wildcard_tlut, iter->second};
  }

  // Single wildcard ignoring the texture hash
  const std::string texture_name_single_wildcard_tex =
      fmt::format("{}_${}_{}", texture_name_details.base_name, texture_name_details.tlut_name,
                  texture_name_details.format_name);
  if (auto iter = s_hires_texture_id_to_arbmipmap.find(texture_name_single_wildcard_tex);
      iter != s_hires_texture_id_to_arbmipmap.end())
  {
    return {texture_name_single_wildcard_tex, iter->second};
  }

  return {"", false};
}
}  // namespace

void HiresTexture::Init()
{
  Update();
}

void HiresTexture::Shutdown()
{
  Clear();
}

void HiresTexture::Update()
{
  if (!g_ActiveConfig.bHiresTextures)
  {
    Clear();
    return;
  }

  const std::string& game_id = SConfig::GetInstance().GetGameID();
  const std::set<std::string> texture_directories =
      GetTextureDirectoriesWithGameId(File::GetUserPath(D_HIRESTEXTURES_IDX), game_id);
  const std::vector<std::string> extensions{".png", ".dds"};

  auto& system = Core::System::GetInstance();

  for (const auto& texture_directory : texture_directories)
  {
    // Watch this directory for any texture reloads
    s_file_library->Watch(texture_directory);

    const auto texture_paths =
        Common::DoFileSearch({texture_directory}, extensions, /*recursive*/ true);

    bool failed_insert = false;
    for (auto& path : texture_paths)
    {
      std::string filename;
      SplitPath(path, nullptr, &filename, nullptr);

      if (filename.substr(0, s_format_prefix.length()) == s_format_prefix)
      {
        const size_t arb_index = filename.rfind("_arb");
        const bool has_arbitrary_mipmaps = arb_index != std::string::npos;
        if (has_arbitrary_mipmaps)
          filename.erase(arb_index, 4);

        const auto [it, inserted] =
            s_hires_texture_id_to_arbmipmap.try_emplace(filename, has_arbitrary_mipmaps);
        if (!inserted)
        {
          failed_insert = true;
        }
        else
        {
          // Since this is just a texture (single file) the mapper doesn't really matter
          // just provide a string
          s_file_library->SetAssetIDMapData(filename, std::map<std::string, std::filesystem::path>{
                                                          {"texture", StringToPath(path)}});

          if (g_ActiveConfig.bCacheHiresTextures)
          {
            auto hires_texture = std::make_shared<HiresTexture>(
                has_arbitrary_mipmaps,
                system.GetCustomAssetLoader().LoadGameTexture(filename, s_file_library));
            s_hires_texture_cache.try_emplace(filename, std::move(hires_texture));
          }
        }
      }
    }

    if (failed_insert)
    {
      ERROR_LOG_FMT(VIDEO, "One or more textures at path '{}' were already inserted",
                    texture_directory);
    }
  }

  if (g_ActiveConfig.bCacheHiresTextures)
  {
    OSD::AddMessage(fmt::format("Loading '{}' custom textures", s_hires_texture_cache.size()),
                    10000);
  }
  else
  {
    OSD::AddMessage(
        fmt::format("Found '{}' custom textures", s_hires_texture_id_to_arbmipmap.size()), 10000);
  }
}

void HiresTexture::Clear()
{
  s_hires_texture_cache.clear();
  s_hires_texture_id_to_arbmipmap.clear();
  s_file_library = std::make_shared<VideoCommon::DirectFilesystemAssetLibrary>();
}

std::shared_ptr<HiresTexture> HiresTexture::Search(const TextureInfo& texture_info)
{
  const auto [base_filename, has_arb_mipmaps] = GetNameArbPair(texture_info);
  if (base_filename == "")
    return nullptr;

  if (auto iter = s_hires_texture_cache.find(base_filename); iter != s_hires_texture_cache.end())
  {
    return iter->second;
  }
  else
  {
    auto& system = Core::System::GetInstance();
    auto hires_texture = std::make_shared<HiresTexture>(
        has_arb_mipmaps,
        system.GetCustomAssetLoader().LoadGameTexture(base_filename, s_file_library));
    if (g_ActiveConfig.bCacheHiresTextures)
    {
      s_hires_texture_cache.try_emplace(base_filename, hires_texture);
    }
    return hires_texture;
  }
}

HiresTexture::HiresTexture(bool has_arbitrary_mipmaps,
                           std::shared_ptr<VideoCommon::GameTextureAsset> asset)
    : m_has_arbitrary_mipmaps(has_arbitrary_mipmaps), m_game_texture(std::move(asset))
{
}

std::set<std::string> GetTextureDirectoriesWithGameId(const std::string& root_directory,
                                                      const std::string& game_id)
{
  std::set<std::string> result;
  const std::string texture_directory = root_directory + game_id;

  if (File::Exists(texture_directory))
  {
    result.insert(texture_directory);
  }
  else
  {
    // If there's no directory with the region-specific ID, look for a 3-character region-free one
    const std::string region_free_directory = root_directory + game_id.substr(0, 3);

    if (File::Exists(region_free_directory))
    {
      result.insert(region_free_directory);
    }
  }

  const auto match_gameid_or_all = [game_id](const std::string& filename) {
    std::string basename;
    SplitPath(filename, nullptr, &basename, nullptr);
    return basename == game_id || basename == game_id.substr(0, 3) || basename == "all";
  };

  // Look for any other directories that might be specific to the given gameid
  const auto files = Common::DoFileSearch({root_directory}, {".txt"}, true);
  for (const auto& file : files)
  {
    if (match_gameid_or_all(file))
    {
      // The following code is used to calculate the top directory
      // of a found gameid.txt file
      // ex:  <root directory>/My folder/gameids/<gameid>.txt
      // would insert "<root directory>/My folder"
      const auto directory_path = file.substr(root_directory.size());
      const std::size_t first_path_separator_position = directory_path.find_first_of(DIR_SEP_CHR);
      result.insert(root_directory + directory_path.substr(0, first_path_separator_position));
    }
  }

  return result;
}
