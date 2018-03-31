// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/GameFile.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/IniFile.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"

#include "Core/Boot/Boot.h"
#include "Core/ConfigManager.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/TitleDatabase.h"

#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/WiiSaveBanner.h"

namespace UICommon
{
static const std::string EMPTY_STRING;

const std::string& GameFile::Lookup(DiscIO::Language language,
                                    const std::map<DiscIO::Language, std::string>& strings)
{
  auto end = strings.end();
  auto it = strings.find(language);
  if (it != end)
    return it->second;

  // English tends to be a good fallback when the requested language isn't available
  if (language != DiscIO::Language::English)
  {
    it = strings.find(DiscIO::Language::English);
    if (it != end)
      return it->second;
  }

  // If English isn't available either, just pick something
  if (!strings.empty())
    return strings.cbegin()->second;

  return EMPTY_STRING;
}

const std::string&
GameFile::LookupUsingConfigLanguage(const std::map<DiscIO::Language, std::string>& strings) const
{
  const bool wii = DiscIO::IsWii(m_platform);
  return Lookup(SConfig::GetInstance().GetCurrentLanguage(wii), strings);
}

GameFile::GameFile(const std::string& path)
    : m_file_path(path), m_region(DiscIO::Region::Unknown), m_country(DiscIO::Country::Unknown)
{
  {
    std::string name, extension;
    SplitPath(m_file_path, nullptr, &name, &extension);
    m_file_name = name + extension;

    std::unique_ptr<DiscIO::Volume> volume(DiscIO::CreateVolumeFromFilename(m_file_path));
    if (volume != nullptr)
    {
      m_platform = volume->GetVolumeType();

      m_short_names = volume->GetShortNames();
      m_long_names = volume->GetLongNames();
      m_short_makers = volume->GetShortMakers();
      m_long_makers = volume->GetLongMakers();
      m_descriptions = volume->GetDescriptions();

      m_region = volume->GetRegion();
      m_country = volume->GetCountry();
      m_blob_type = volume->GetBlobType();
      m_file_size = volume->GetRawSize();
      m_volume_size = volume->GetSize();

      m_internal_name = volume->GetInternalName();
      m_game_id = volume->GetGameID();
      m_title_id = volume->GetTitleID().value_or(0);
      m_maker_id = volume->GetMakerID();
      m_revision = volume->GetRevision().value_or(0);
      m_disc_number = volume->GetDiscNumber().value_or(0);
      m_apploader_date = volume->GetApploaderDate();

      m_volume_banner.buffer = volume->GetBanner(&m_volume_banner.width, &m_volume_banner.height);

      m_valid = true;
    }
  }

  if (!IsValid() && IsElfOrDol())
  {
    m_valid = true;
    m_file_size = File::GetSize(m_file_path);
    m_platform = DiscIO::Platform::ELFOrDOL;
    m_blob_type = DiscIO::BlobType::DIRECTORY;
  }
}

bool GameFile::IsValid() const
{
  if (!m_valid)
    return false;

  if (m_platform == DiscIO::Platform::WiiWAD && !IOS::ES::IsChannel(m_title_id))
    return false;

  return true;
}

bool GameFile::CustomNameChanged(const Core::TitleDatabase& title_database)
{
  const auto type = m_platform == DiscIO::Platform::WiiWAD ?
                        Core::TitleDatabase::TitleType::Channel :
                        Core::TitleDatabase::TitleType::Other;
  m_pending.custom_name = title_database.GetTitleName(m_game_id, type);
  return m_custom_name != m_pending.custom_name;
}

void GameFile::CustomNameCommit()
{
  m_custom_name = std::move(m_pending.custom_name);
}

bool GameFile::EmuStateChanged()
{
  IniFile ini = SConfig::LoadGameIni(m_game_id, m_revision);
  ini.GetIfExists("EmuState", "EmulationStateId", &m_pending.emu_state.rating, 0);
  ini.GetIfExists("EmuState", "EmulationIssues", &m_pending.emu_state.issues, std::string());
  return m_emu_state != m_pending.emu_state;
}

void GameFile::EmuStateCommit()
{
  m_emu_state = std::move(m_pending.emu_state);
}

void GameFile::EmuState::DoState(PointerWrap& p)
{
  p.Do(rating);
  p.Do(issues);
}

void GameBanner::DoState(PointerWrap& p)
{
  p.Do(buffer);
  p.Do(width);
  p.Do(height);
}

void GameFile::DoState(PointerWrap& p)
{
  p.Do(m_valid);
  p.Do(m_file_path);
  p.Do(m_file_name);

  p.Do(m_file_size);
  p.Do(m_volume_size);

  p.Do(m_short_names);
  p.Do(m_long_names);
  p.Do(m_short_makers);
  p.Do(m_long_makers);
  p.Do(m_descriptions);
  p.Do(m_internal_name);
  p.Do(m_game_id);
  p.Do(m_title_id);
  p.Do(m_maker_id);

  p.Do(m_region);
  p.Do(m_country);
  p.Do(m_platform);
  p.Do(m_blob_type);
  p.Do(m_revision);
  p.Do(m_disc_number);
  p.Do(m_apploader_date);

  m_volume_banner.DoState(p);
  m_emu_state.DoState(p);
  p.Do(m_custom_name);
}

bool GameFile::IsElfOrDol() const
{
  if (m_file_path.size() < 4)
    return false;

  std::string name_end = m_file_path.substr(m_file_path.size() - 4);
  std::transform(name_end.begin(), name_end.end(), name_end.begin(), ::tolower);
  return name_end == ".elf" || name_end == ".dol";
}

bool GameFile::BannerChanged()
{
  // Wii banners can only be read if there is a save file.
  // In case the cache was created without a save file existing,
  // let's try reading the save file again, because it might exist now.

  if (!m_volume_banner.empty())
    return false;
  if (!DiscIO::IsWii(m_platform))
    return false;

  m_pending.volume_banner.buffer =
      DiscIO::WiiSaveBanner(m_title_id)
          .GetBanner(&m_pending.volume_banner.width, &m_pending.volume_banner.height);

  // We only reach here if the old banner was empty, so if the new banner isn't empty,
  // the new banner is guaranteed to be different from the old banner
  return !m_pending.volume_banner.buffer.empty();
}

void GameFile::BannerCommit()
{
  m_volume_banner = std::move(m_pending.volume_banner);
}

const std::string& GameFile::GetName(bool long_name) const
{
  if (!m_custom_name.empty())
    return m_custom_name;

  const std::string& name = long_name ? GetLongName() : GetShortName();
  if (!name.empty())
    return name;

  // No usable name, return filename (better than nothing)
  return m_file_name;
}

const std::string& GameFile::GetMaker(bool long_maker) const
{
  const std::string& maker = long_maker ? GetLongMaker() : GetShortMaker();
  if (!maker.empty())
    return maker;

  if (m_game_id.size() >= 6)
    return DiscIO::GetCompanyFromID(m_maker_id);

  return EMPTY_STRING;
}

std::vector<DiscIO::Language> GameFile::GetLanguages() const
{
  std::vector<DiscIO::Language> languages;
  // TODO: What if some languages don't have long names but have other strings?
  for (std::pair<DiscIO::Language, std::string> name : m_long_names)
    languages.push_back(name.first);
  return languages;
}

std::string GameFile::GetUniqueIdentifier() const
{
  const DiscIO::Language lang = DiscIO::Language::English;
  std::vector<std::string> info;
  if (!GetGameID().empty())
    info.push_back(GetGameID());
  if (GetRevision() != 0)
    info.push_back("Revision " + std::to_string(GetRevision()));

  std::string name(GetLongName(lang));
  if (name.empty())
    name = GetName();

  int disc_number = GetDiscNumber() + 1;

  std::string lower_name = name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
  if (disc_number > 1 &&
      lower_name.find(StringFromFormat("disc %i", disc_number)) == std::string::npos &&
      lower_name.find(StringFromFormat("disc%i", disc_number)) == std::string::npos)
  {
    std::string disc_text = "Disc ";
    info.push_back(disc_text + std::to_string(disc_number));
  }
  if (info.empty())
    return name;
  std::ostringstream ss;
  std::copy(info.begin(), info.end() - 1, std::ostream_iterator<std::string>(ss, ", "));
  ss << info.back();
  return name + " (" + ss.str() + ")";
}

std::string GameFile::GetWiiFSPath() const
{
  ASSERT(DiscIO::IsWii(m_platform));
  return Common::GetTitleDataPath(m_title_id, Common::FROM_CONFIGURED_ROOT);
}

}  // namespace UICommon
