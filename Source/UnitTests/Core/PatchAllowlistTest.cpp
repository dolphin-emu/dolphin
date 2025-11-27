// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS

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
#include "Common/IOFile.h"
#include "Common/IniFile.h"
#include "Common/JsonUtil.h"
#include "Core/AchievementManager.h"
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

template <typename T>
void ReadVerified(const Common::IniFile& ini, const std::string& filename,
                  const std::string& section, bool enabled, std::vector<T>* codes);

TEST(PatchAllowlist, VerifyHashes)
{
  // Iterate over GameSettings directory
  picojson::object new_allowlist;
  std::string cur_directory = File::GetExeDirectory()
#if defined(__APPLE__)
                              + DIR_SEP "Tests"  // FIXME: Ugly hack.
#endif
      ;
  std::string sys_directory = cur_directory + DIR_SEP "Sys";
  auto directory =
      File::ScanDirectoryTree(fmt::format("{}{}GameSettings", sys_directory, DIR_SEP), false);
  for (const auto& file : directory.children)
  {
    // Load ini file
    picojson::object approved;
    Common::IniFile ini_file;
    ini_file.Load(file.physicalName, true);
    std::string game_id = file.virtualName.substr(0, file.virtualName.find_first_of('.'));
    std::vector<PatchEngine::Patch> patches;
    PatchEngine::LoadPatchSection("OnFrame", &patches, ini_file, Common::IniFile());
    std::vector<Gecko::GeckoCode> geckos = Gecko::LoadCodes(Common::IniFile(), ini_file);
    std::vector<ActionReplay::ARCode> action_replays =
        ActionReplay::LoadCodes(Common::IniFile(), ini_file);
    // Filter patches for RetroAchievements approved
    ReadVerified<PatchEngine::Patch>(ini_file, game_id, "Patches_RetroAchievements_Verified", true,
                                     &patches);
    ReadVerified<Gecko::GeckoCode>(ini_file, game_id, "Gecko_RetroAchievements_Verified", true,
                                   &geckos);
    ReadVerified<ActionReplay::ARCode>(ini_file, game_id, "AR_RetroAchievements_Verified", true,
                                       &action_replays);
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
      approved[patch.name] = picojson::value(Common::SHA1::DigestToString(digest));
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
      approved[code.name] = picojson::value(Common::SHA1::DigestToString(digest));
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
      approved[code.name] = picojson::value(Common::SHA1::DigestToString(digest));
    }
    // Add approved patches and codes to tree
    if (!approved.empty())
      new_allowlist[game_id] = picojson::value(approved);
  }

  // Hash new allowlist
  std::string new_allowlist_str = picojson::value(new_allowlist).serialize();
  auto context = Common::SHA1::CreateContext();
  context->Update(new_allowlist_str);
  auto digest = context->Finish();
  if (digest != AchievementManager::APPROVED_LIST_HASH)
  {
    ADD_FAILURE() << "Approved list hash does not match the one in AchievementMananger."
                  << std::endl
                  << "Please update APPROVED_LIST_HASH to the following:" << std::endl
                  << Common::SHA1::DigestToString(digest);
  }
  // Compare with old allowlist
  static constexpr std::string_view APPROVED_LIST_FILENAME = "ApprovedInis.json";
  std::string old_allowlist;
  std::string error;
  const auto& list_filepath = fmt::format("{}{}{}", sys_directory, DIR_SEP, APPROVED_LIST_FILENAME);
  if (!File::ReadFileToString(list_filepath, old_allowlist) || old_allowlist != new_allowlist_str)
  {
    static constexpr std::string_view NEW_APPROVED_LIST_FILENAME = "New-ApprovedInis.json";
    const auto& new_list_filepath =
        fmt::format("{}{}{}", sys_directory, DIR_SEP, NEW_APPROVED_LIST_FILENAME);
    if (!JsonToFile(new_list_filepath, picojson::value(new_allowlist), false))
    {
      ADD_FAILURE() << "Failed to write new approved list to " << list_filepath;
    }
    ADD_FAILURE() << "Approved list needs to be updated. Please run this test in your" << std::endl
                  << "local environment and copy" << std::endl
                  << new_list_filepath << std::endl
                  << "to Data/Sys/ApprovedInis.json to pass this test.";
  }
}

template <typename T>
void ReadVerified(const Common::IniFile& ini, const std::string& filename,
                  const std::string& section, bool enabled, std::vector<T>* codes)
{
  for (auto& code : *codes)
    code.enabled = false;

  std::vector<std::string> lines;
  ini.GetLines(section, &lines, false);

  for (const std::string& line : lines)
  {
    if (line.empty() || line[0] != '$')
      continue;

    bool found = false;
    for (T& code : *codes)
    {
      // Exclude the initial '$' from the comparison.
      if (line.compare(1, std::string::npos, code.name) == 0)
      {
        code.enabled = enabled;
        found = true;
      }
    }
    if (!found)
    {
      // Report: approved patch in ini doesn't actually exist
      ADD_FAILURE() << "Code with approval not found" << std::endl
                    << "Game ID: " << filename << std::endl
                    << "Name: \"" << line << "\"";
    }
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
