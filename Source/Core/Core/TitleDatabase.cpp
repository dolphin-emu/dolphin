// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/TitleDatabase.h"

#include <cstddef>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <utility>

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/Enums.h"

namespace Core
{
static std::string GetLanguageCode(DiscIO::Language language)
{
  switch (language)
  {
  case DiscIO::Language::LANGUAGE_JAPANESE:
    return "ja";
  case DiscIO::Language::LANGUAGE_ENGLISH:
    return "en";
  case DiscIO::Language::LANGUAGE_GERMAN:
    return "de";
  case DiscIO::Language::LANGUAGE_FRENCH:
    return "fr";
  case DiscIO::Language::LANGUAGE_SPANISH:
    return "es";
  case DiscIO::Language::LANGUAGE_ITALIAN:
    return "it";
  case DiscIO::Language::LANGUAGE_DUTCH:
    return "nl";
  case DiscIO::Language::LANGUAGE_SIMPLIFIED_CHINESE:
    return "zh_CN";
  case DiscIO::Language::LANGUAGE_TRADITIONAL_CHINESE:
    return "zh_TW";
  case DiscIO::Language::LANGUAGE_KOREAN:
    return "ko";
  default:
    return "en";
  }
}

using Map = std::unordered_map<std::string, std::string>;

// Note that this function will not overwrite entries that already are in the maps
static bool LoadMap(const std::string& file_path, Map& map,
                    std::function<bool(const std::string& game_id)> predicate)
{
  std::ifstream txt;
  File::OpenFStream(txt, file_path, std::ios::in);

  if (!txt.is_open())
    return false;

  std::string line;
  while (std::getline(txt, line))
  {
    if (line.empty())
      continue;

    const size_t equals_index = line.find('=');
    if (equals_index != std::string::npos)
    {
      const std::string game_id = StripSpaces(line.substr(0, equals_index));
      if (game_id.length() >= 4 && predicate(game_id))
        map.emplace(game_id, StripSpaces(line.substr(equals_index + 1)));
    }
  }
  return true;
}

// This should only be used with the common game ID format (used by WiiTDBs), not Dolphin's.
// Otherwise, TurboGrafx-16 VC games (with the system ID P) will be misdetected as GameCube titles.
// The formats differ in that Dolphin's uses 6 characters for non-disc titles instead of 4.
static bool IsGCTitle(const std::string& game_id)
{
  const char system_id = game_id[0];
  return game_id.length() == 6 &&
         (system_id == 'G' || system_id == 'D' || system_id == 'U' || system_id == 'P');
}

static bool IsWiiTitle(const std::string& game_id)
{
  // Assume that any non-GameCube title is a Wii title.
  return !IsGCTitle(game_id);
}

static bool IsJapaneseGCTitle(const std::string& game_id)
{
  return IsGCTitle(game_id) && DiscIO::CountrySwitch(game_id[3]) == DiscIO::Country::COUNTRY_JAPAN;
}

static bool IsNonJapaneseGCTitle(const std::string& game_id)
{
  return IsGCTitle(game_id) && DiscIO::CountrySwitch(game_id[3]) != DiscIO::Country::COUNTRY_JAPAN;
}

// Note that this function will not overwrite entries that already are in the maps
static bool LoadMap(const std::string& file_path, Map& gc_map, Map& wii_map)
{
  Map map;
  if (!LoadMap(file_path, map, [](const auto& game_id) { return true; }))
    return false;

  for (auto& entry : map)
  {
    auto& destination_map = IsGCTitle(entry.first) ? gc_map : wii_map;
    destination_map.emplace(std::move(entry));
  }
  return true;
}

TitleDatabase::TitleDatabase()
{
  // Load the user databases.
  const std::string& load_directory = File::GetUserPath(D_LOAD_IDX);
  if (!LoadMap(load_directory + "wiitdb.txt", m_gc_title_map, m_wii_title_map))
    LoadMap(load_directory + "titles.txt", m_gc_title_map, m_wii_title_map);

  if (!SConfig::GetInstance().m_use_builtin_title_database)
    return;

  // Load the database in the console language.
  // Note: The GameCube language setting can't be set to Japanese,
  // so instead, we use Japanese names iff the games are NTSC-J.
  const std::string gc_code = GetLanguageCode(SConfig::GetInstance().GetCurrentLanguage(false));
  const std::string wii_code = GetLanguageCode(SConfig::GetInstance().GetCurrentLanguage(true));
  LoadMap(File::GetSysDirectory() + "wiitdb-ja.txt", m_gc_title_map, IsJapaneseGCTitle);
  if (gc_code != "en")
  {
    LoadMap(File::GetSysDirectory() + "wiitdb-" + gc_code + ".txt", m_gc_title_map,
            IsNonJapaneseGCTitle);
  }
  if (wii_code != "en")
    LoadMap(File::GetSysDirectory() + "wiitdb-" + wii_code + ".txt", m_wii_title_map, IsWiiTitle);

  // Load the English database as the base database.
  LoadMap(File::GetSysDirectory() + "wiitdb-en.txt", m_gc_title_map, m_wii_title_map);

  // Titles that cannot be part of the Wii TDB,
  // but common enough to justify having entries for them.

  // i18n: "Wii Menu" (or System Menu) refers to the Wii's main menu,
  // which is (usually) the first thing users see when a Wii console starts.
  m_wii_title_map.emplace("0000000100000002", GetStringT("Wii Menu"));
  for (const auto& id : {"HAXX", "JODI", "00010001af1bf516", "LULZ", "OHBC"})
    m_wii_title_map.emplace(id, "The Homebrew Channel");
}

TitleDatabase::~TitleDatabase() = default;

std::string TitleDatabase::GetTitleName(const std::string& game_id, TitleType type) const
{
  const auto& map = IsWiiTitle(game_id) ? m_wii_title_map : m_gc_title_map;
  const std::string key =
      type == TitleType::Channel && game_id.length() == 6 ? game_id.substr(0, 4) : game_id;
  const auto iterator = map.find(key);
  return iterator != map.end() ? iterator->second : "";
}

std::string TitleDatabase::GetChannelName(u64 title_id) const
{
  const std::string id{
      {static_cast<char>((title_id >> 24) & 0xff), static_cast<char>((title_id >> 16) & 0xff),
       static_cast<char>((title_id >> 8) & 0xff), static_cast<char>(title_id & 0xff)}};
  return GetTitleName(id, TitleType::Channel);
}

std::string TitleDatabase::Describe(const std::string& game_id, TitleType type) const
{
  const std::string title_name = GetTitleName(game_id, type);
  if (title_name.empty())
    return game_id;
  return StringFromFormat("%s (%s)", title_name.c_str(), game_id.c_str());
}
}  // namespace Core
