// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <picojson.h>

#include "Common/BitUtils.h"
#include "Common/CommonPaths.h"
#include "Common/Crypto/SHA1.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/JsonUtil.h"
#include "Core/CheatCodes.h"
#include "Core/PatchEngine.h"

struct GameHashes
{
  std::string game_title;
  std::map<std::string /*hash*/, std::string /*patch name*/> hashes;
};

TEST(PatchAllowlist, VerifyHashes)
{
  // Load allowlist
  static constexpr std::string_view APPROVED_LIST_FILENAME = "ApprovedInis.json";
  picojson::value json_tree;
  std::string error;
  std::string cur_directory = File::GetExeDirectory()
#if defined(__APPLE__)
                              + DIR_SEP "Tests"  // FIXME: Ugly hack.
#endif
      ;
  std::string sys_directory = cur_directory + DIR_SEP "Sys";
  const auto& list_filepath = fmt::format("{}{}{}", sys_directory, DIR_SEP, APPROVED_LIST_FILENAME);
  ASSERT_TRUE(JsonFromFile(list_filepath, &json_tree, &error))
      << "Failed to open file at " << list_filepath;
  // Parse allowlist - Map<game id, Map<hash, name>
  ASSERT_TRUE(json_tree.is<picojson::object>());
  std::map<std::string /*ID*/, GameHashes> allow_list;
  for (const auto& entry : json_tree.get<picojson::object>())
  {
    ASSERT_TRUE(entry.second.is<picojson::object>());
    GameHashes& game_entry = allow_list[entry.first];
    for (const auto& line : entry.second.get<picojson::object>())
    {
      ASSERT_TRUE(line.second.is<std::string>());
      if (line.first == "title")
        game_entry.game_title = line.second.get<std::string>();
      else
        game_entry.hashes[line.first] = line.second.get<std::string>();
    }
  }
  // Iterate over GameSettings directory
  auto directory =
      File::ScanDirectoryTree(fmt::format("{}{}GameSettings", sys_directory, DIR_SEP), false);
  for (const auto& file : directory.children)
  {
    // Load ini file
    Common::IniFile ini_file;
    ini_file.Load(file.physicalName, true);
    std::string game_id = file.virtualName.substr(0, file.virtualName.find_first_of('.'));
    std::vector<PatchEngine::Patch> patches;
    PatchEngine::LoadPatchSection("OnFrame", &patches, ini_file, Common::IniFile());
    // Filter patches for RetroAchievements approved
    ReadEnabledOrDisabled<PatchEngine::Patch>(ini_file, "OnFrame", false, &patches);
    ReadEnabledOrDisabled<PatchEngine::Patch>(ini_file, "Patches_RetroAchievements_Verified", true,
                                              &patches);
    // Get game section from allow list
    auto game_itr = allow_list.find(game_id);
    // Iterate over approved patches
    for (const auto& patch : patches)
    {
      if (!patch.enabled)
        continue;
      // Hash patch
      auto context = Common::SHA1::CreateContext();
      context->Update(Common::BitCastToArray<u8>(static_cast<u64>(patch.entries.size())));
      for (const auto& entry : patch.entries)
      {
        context->Update(Common::BitCastToArray<u8>(entry.type));
        context->Update(Common::BitCastToArray<u8>(entry.address));
        context->Update(Common::BitCastToArray<u8>(entry.value));
        context->Update(Common::BitCastToArray<u8>(entry.comparand));
        context->Update(Common::BitCastToArray<u8>(entry.conditional));
      }
      auto digest = context->Finish();
      std::string hash = Common::SHA1::DigestToString(digest);
      // Check patch in list
      if (game_itr == allow_list.end())
      {
        // Report: no patches in game found in list
        ADD_FAILURE() << "Approved hash missing from list." << std::endl
                      << "Game ID: " << game_id << std::endl
                      << "Patch: \"" << hash << "\" : \"" << patch.name << "\"";
        continue;
      }
      auto hash_itr = game_itr->second.hashes.find(hash);
      if (hash_itr == game_itr->second.hashes.end())
      {
        // Report: patch not found in list
        ADD_FAILURE() << "Approved hash missing from list." << std::endl
                      << "Game ID: " << game_id << ":" << game_itr->second.game_title << std::endl
                      << "Patch: \"" << hash << "\" : \"" << patch.name << "\"";
      }
      else
      {
        // Remove patch from map if found
        game_itr->second.hashes.erase(hash_itr);
      }
    }
    // Report missing patches in map
    if (game_itr == allow_list.end())
      continue;
    for (auto& remaining_hashes : game_itr->second.hashes)
    {
      ADD_FAILURE() << "Hash in list not approved in ini." << std::endl
                    << "Game ID: " << game_id << ":" << game_itr->second.game_title << std::endl
                    << "Patch: " << remaining_hashes.second << ":" << remaining_hashes.first;
    }
    //    Remove section from map
    allow_list.erase(game_itr);
  }
  //  Report remaining sections in map
  for (auto& remaining_games : allow_list)
  {
    ADD_FAILURE() << "Game in list has no ini file." << std::endl
                  << "Game ID: " << remaining_games.first << ":"
                  << remaining_games.second.game_title;
  }
}
