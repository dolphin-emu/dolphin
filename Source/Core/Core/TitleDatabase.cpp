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
static const std::string EMPTY_STRING;

using Map = std::unordered_map<std::string, std::string>;

static Map LoadMap(const std::string& file_path)
{
  Map map;

  std::ifstream txt;
  File::OpenFStream(txt, file_path, std::ios::in);

  std::string line;
  while (std::getline(txt, line))
  {
    if (line.empty())
      continue;

    const size_t equals_index = line.find('=');
    if (equals_index != std::string::npos)
    {
      const std::string game_id = StripSpaces(line.substr(0, equals_index));
      if (game_id.length() >= 4)
        map.emplace(game_id, StripSpaces(line.substr(equals_index + 1)));
    }
  }
  return map;
}

void TitleDatabase::AddLazyMap(DiscIO::Language language, const std::string& language_code)
{
  m_title_maps[language] = [language_code]() -> Map {
    return LoadMap(File::GetSysDirectory() + "wiitdb-" + language_code + ".txt");
  };
}

TitleDatabase::TitleDatabase()
{
  // User database
  const std::string& load_directory = File::GetUserPath(D_LOAD_IDX);
  m_user_title_map = LoadMap(load_directory + "wiitdb.txt");
  if (m_user_title_map.empty())
    m_user_title_map = LoadMap(load_directory + "titles.txt");

  // Pre-defined databases (one per language)
  AddLazyMap(DiscIO::Language::Japanese, "ja");
  AddLazyMap(DiscIO::Language::English, "en");
  AddLazyMap(DiscIO::Language::German, "de");
  AddLazyMap(DiscIO::Language::French, "fr");
  AddLazyMap(DiscIO::Language::Spanish, "es");
  AddLazyMap(DiscIO::Language::Italian, "it");
  AddLazyMap(DiscIO::Language::Dutch, "nl");
  AddLazyMap(DiscIO::Language::SimplifiedChinese, "zh_CN");
  AddLazyMap(DiscIO::Language::TraditionalChinese, "zh_TW");
  AddLazyMap(DiscIO::Language::Korean, "ko");

  // Titles that aren't part of the Wii TDB, but common enough to justify having entries for them.

  // i18n: "Wii Menu" (or System Menu) refers to the Wii's main menu,
  // which is (usually) the first thing users see when a Wii console starts.
  m_base_map.emplace("0000000100000002", Common::GetStringT("Wii Menu"));
  for (const auto& id : {"HAXX", "00010001af1bf516"})
    m_base_map.emplace(id, "The Homebrew Channel");
}

TitleDatabase::TitleDatabase(const std::string& language)
{
  // User database
  const std::string& load_directory = File::GetUserPath(D_LOAD_IDX);
  m_user_title_map = LoadMap(load_directory + "wiitdb.txt");
  if (m_user_title_map.empty())
    m_user_title_map = LoadMap(load_directory + "titles.txt");

  Map my_tdb = LoadMap(File::GetSysDirectory() + "wiitdb-" + language + ".txt");
  m_user_title_map.insert(my_tdb.begin(), my_tdb.end());

  // Pre-defined databases (one per language)
  AddLazyMap(DiscIO::Language::Japanese, "ja");
  AddLazyMap(DiscIO::Language::English, "en");
  AddLazyMap(DiscIO::Language::German, "de");
  AddLazyMap(DiscIO::Language::French, "fr");
  AddLazyMap(DiscIO::Language::Spanish, "es");
  AddLazyMap(DiscIO::Language::Italian, "it");
  AddLazyMap(DiscIO::Language::Dutch, "nl");
  AddLazyMap(DiscIO::Language::SimplifiedChinese, "zh_CN");
  AddLazyMap(DiscIO::Language::TraditionalChinese, "zh_TW");
  AddLazyMap(DiscIO::Language::Korean, "ko");

  // Titles that aren't part of the Wii TDB, but common enough to justify having entries for them.

  // i18n: "Wii Menu" (or System Menu) refers to the Wii's main menu,
  // which is (usually) the first thing users see when a Wii console starts.
  m_base_map.emplace("0000000100000002", Common::GetStringT("Wii Menu"));
  for (const auto& id : {"HAXX", "00010001af1bf516"})
    m_base_map.emplace(id, "The Homebrew Channel");
}

TitleDatabase::~TitleDatabase() = default;

const std::string& TitleDatabase::GetTitleName(const std::string& gametdb_id,
                                               DiscIO::Language language) const
{
  auto it = m_user_title_map.find(gametdb_id);
  if (it != m_user_title_map.end())
    return it->second;

  if (!SConfig::GetInstance().m_use_builtin_title_database)
    return EMPTY_STRING;

  const Map& map = *m_title_maps.at(language);
  it = map.find(gametdb_id);
  if (it != map.end())
    return it->second;

  if (language != DiscIO::Language::English)
  {
    const Map& english_map = *m_title_maps.at(DiscIO::Language::English);
    it = english_map.find(gametdb_id);
    if (it != english_map.end())
      return it->second;
  }

  it = m_base_map.find(gametdb_id);
  if (it != m_base_map.end())
    return it->second;

  return EMPTY_STRING;
}

const std::string& TitleDatabase::GetChannelName(u64 title_id, DiscIO::Language language) const
{
  const std::string id{
      {static_cast<char>((title_id >> 24) & 0xff), static_cast<char>((title_id >> 16) & 0xff),
       static_cast<char>((title_id >> 8) & 0xff), static_cast<char>(title_id & 0xff)}};
  return GetTitleName(id, language);
}

std::string TitleDatabase::Describe(const std::string& gametdb_id, DiscIO::Language language) const
{
  const std::string& title_name = GetTitleName(gametdb_id, language);
  if (title_name.empty())
    return gametdb_id;
  return StringFromFormat("%s (%s)", title_name.c_str(), gametdb_id.c_str());
}
}  // namespace Core
