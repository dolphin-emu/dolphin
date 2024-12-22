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
#include "Core/ActionReplay.h"
#include "Core/CheatCodes.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/PatchEngine.h"

struct GameHashes
{
  std::string game_title;
  std::map<std::string /*hash*/, std::string /*patch name*/> hashes;
};

using AllowList = std::map<std::string /*ID*/, GameHashes>;

void CheckHash(const std::string& game_id, GameHashes* game_hashes, const std::string& hash,
               const std::string& patch_name);

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
  // Parse allowlist - Map<game id, Map<hash, name>>
  ASSERT_TRUE(json_tree.is<picojson::object>());
  AllowList allow_list;
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
    std::vector<Gecko::GeckoCode> geckos = Gecko::LoadCodes(Common::IniFile(), ini_file);
    std::vector<ActionReplay::ARCode> action_replays =
        ActionReplay::LoadCodes(Common::IniFile(), ini_file);
    // Filter patches for RetroAchievements approved
    for (auto& patch : patches)
      patch.enabled = false;
    ReadEnabledOrDisabled<PatchEngine::Patch>(ini_file, "Patches_RetroAchievements_Verified", true,
                                              &patches);
    for (auto& code : geckos)
      code.enabled = false;
    ReadEnabledOrDisabled<Gecko::GeckoCode>(ini_file, "Gecko_RetroAchievements_Verified", true,
                                            &geckos);
    for (auto& code : action_replays)
      code.enabled = false;
    ReadEnabledOrDisabled<ActionReplay::ARCode>(ini_file, "AR_RetroAchievements_Verified", true,
                                                &action_replays);
    // Get game section from allow list
    auto game_itr = allow_list.find(game_id);
    bool itr_end = (game_itr == allow_list.end());
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
      CheckHash(game_id, itr_end ? nullptr : &game_itr->second,
                Common::SHA1::DigestToString(digest), patch.name);
    }
    // Iterate over approved geckos
    for (const auto& code : geckos)
    {
      if (!code.enabled)
        continue;
      // Hash patch
      auto context = Common::SHA1::CreateContext();
      context->Update(Common::BitCastToArray<u8>(static_cast<u64>(code.codes.size())));
      for (const auto& entry : code.codes)
      {
        context->Update(Common::BitCastToArray<u8>(entry.address));
        context->Update(Common::BitCastToArray<u8>(entry.data));
      }
      auto digest = context->Finish();
      CheckHash(game_id, itr_end ? nullptr : &game_itr->second,
                Common::SHA1::DigestToString(digest), code.name);
    }
    // Iterate over approved AR codes
    for (const auto& code : action_replays)
    {
      if (!code.enabled)
        continue;
      // Hash patch
      auto context = Common::SHA1::CreateContext();
      context->Update(Common::BitCastToArray<u8>(static_cast<u64>(code.ops.size())));
      for (const auto& entry : code.ops)
      {
        context->Update(Common::BitCastToArray<u8>(entry.cmd_addr));
        context->Update(Common::BitCastToArray<u8>(entry.value));
      }
      auto digest = context->Finish();
      CheckHash(game_id, itr_end ? nullptr : &game_itr->second,
                Common::SHA1::DigestToString(digest), code.name);
    }
    // Report missing patches in map
    if (itr_end)
      continue;
    for (auto& remaining_hashes : game_itr->second.hashes)
    {
      ADD_FAILURE() << "Hash in list not approved in ini." << std::endl
                    << "Game ID: " << game_id << ":" << game_itr->second.game_title << std::endl
                    << "Code: " << remaining_hashes.second << ":" << remaining_hashes.first;
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

void CheckHash(const std::string& game_id, GameHashes* game_hashes, const std::string& hash,
               const std::string& patch_name)
{
  // Check patch in list
  if (game_hashes == nullptr)
  {
    // Report: no patches in game found in list
    ADD_FAILURE() << "Approved hash missing from list." << std::endl
                  << "Game ID: " << game_id << std::endl
                  << "Code: \"" << hash << "\": \"" << patch_name << "\"";
    return;
  }
  auto hash_itr = game_hashes->hashes.find(hash);
  if (hash_itr == game_hashes->hashes.end())
  {
    // Report: patch not found in list
    ADD_FAILURE() << "Approved hash missing from list." << std::endl
                  << "Game ID: " << game_id << ":" << game_hashes->game_title << std::endl
                  << "Code: \"" << hash << "\": \"" << patch_name << "\"";
  }
  else
  {
    // Remove patch from map if found
    game_hashes->hashes.erase(hash_itr);
  }
}
