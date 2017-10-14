// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wx/app.h>
#include <wx/bitmap.h>
#include <wx/filefn.h>
#include <wx/image.h>
#include <wx/toplevel.h>

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

#include "DolphinWX/ISOFile.h"
#include "DolphinWX/WxUtils.h"

static std::string GetLanguageString(DiscIO::Language language,
                                     std::map<DiscIO::Language, std::string> strings)
{
  auto end = strings.end();
  auto it = strings.find(language);
  if (it != end)
    return it->second;

  // English tends to be a good fallback when the requested language isn't available
  if (language != DiscIO::Language::LANGUAGE_ENGLISH)
  {
    it = strings.find(DiscIO::Language::LANGUAGE_ENGLISH);
    if (it != end)
      return it->second;
  }

  // If English isn't available either, just pick something
  if (!strings.empty())
    return strings.cbegin()->second;

  return "";
}

GameListItem::GameListItem(const std::string& filename)
    : m_file_name(filename), m_region(DiscIO::Region::UNKNOWN_REGION),
      m_country(DiscIO::Country::COUNTRY_UNKNOWN)
{
  {
    std::unique_ptr<DiscIO::Volume> volume(DiscIO::CreateVolumeFromFilename(m_file_name));
    if (volume != nullptr)
    {
      m_platform = volume->GetVolumeType();

      m_descriptions = volume->GetDescriptions();
      m_names = volume->GetLongNames();
      if (m_names.empty())
        m_names = volume->GetShortNames();
      m_company = GetLanguageString(DiscIO::Language::LANGUAGE_ENGLISH, volume->GetLongMakers());
      if (m_company.empty())
        m_company = GetLanguageString(DiscIO::Language::LANGUAGE_ENGLISH, volume->GetShortMakers());

      m_region = volume->GetRegion();
      m_country = volume->GetCountry();
      m_blob_type = volume->GetBlobType();
      m_file_size = volume->GetRawSize();
      m_volume_size = volume->GetSize();

      m_game_id = volume->GetGameID();
      m_title_id = volume->GetTitleID().value_or(0);
      m_disc_number = volume->GetDiscNumber().value_or(0);
      m_revision = volume->GetRevision().value_or(0);

      auto& banner = m_volume_banner;
      std::vector<u32> buffer = volume->GetBanner(&banner.width, &banner.height);
      ReadVolumeBanner(&banner.buffer, buffer, banner.width, banner.height);

      m_valid = true;
    }
  }

  if (m_company.empty() && m_game_id.size() >= 6)
    m_company = DiscIO::GetCompanyFromID(m_game_id.substr(4, 2));

  if (!IsValid() && IsElfOrDol())
  {
    m_valid = true;
    m_file_size = File::GetSize(m_file_name);
    m_platform = DiscIO::Platform::ELF_DOL;
    m_blob_type = DiscIO::BlobType::DIRECTORY;

    std::string path, name;
    SplitPath(m_file_name, &path, &name, nullptr);

    // A bit like the Homebrew Channel icon, except there can be multiple files
    // in a folder with their own icons. Useful for those who don't want to have
    // a Homebrew Channel-style folder structure.
    if (SetWxBannerFromPNGFile(path + name + ".png"))
      return;

    // Homebrew Channel icon. The most typical icon format for DOLs and ELFs.
    if (SetWxBannerFromPNGFile(path + "icon.png"))
      return;
  }
  else
  {
    // Volume banner. Typical for everything that isn't a DOL or ELF.
    SetWxBannerFromRaw(m_volume_banner);
  }
}

bool GameListItem::IsValid() const
{
  if (!m_valid)
    return false;

  if (m_platform == DiscIO::Platform::WII_WAD && !IOS::ES::IsChannel(m_title_id))
    return false;

  return true;
}

bool GameListItem::CustomNameChanged(const Core::TitleDatabase& title_database)
{
  const auto type = m_platform == DiscIO::Platform::WII_WAD ?
                        Core::TitleDatabase::TitleType::Channel :
                        Core::TitleDatabase::TitleType::Other;
  m_pending.custom_name = title_database.GetTitleName(m_game_id, type);
  return m_custom_name != m_pending.custom_name;
}

void GameListItem::CustomNameCommit()
{
  m_custom_name = std::move(m_pending.custom_name);
}

bool GameListItem::EmuStateChanged()
{
  IniFile ini = SConfig::LoadGameIni(m_game_id, m_revision);
  ini.GetIfExists("EmuState", "EmulationStateId", &m_pending.emu_state.rating, 0);
  ini.GetIfExists("EmuState", "EmulationIssues", &m_pending.emu_state.issues, std::string());
  return m_emu_state != m_pending.emu_state;
}

void GameListItem::EmuStateCommit()
{
  m_emu_state = std::move(m_pending.emu_state);
}

void GameListItem::EmuState::DoState(PointerWrap& p)
{
  p.Do(rating);
  p.Do(issues);
}

void GameListItem::Banner::DoState(PointerWrap& p)
{
  p.Do(buffer);
  p.Do(width);
  p.Do(height);
}

void GameListItem::DoState(PointerWrap& p)
{
  p.Do(m_valid);
  p.Do(m_file_name);
  p.Do(m_file_size);
  p.Do(m_volume_size);
  p.Do(m_names);
  p.Do(m_descriptions);
  p.Do(m_company);
  p.Do(m_game_id);
  p.Do(m_title_id);
  p.Do(m_region);
  p.Do(m_country);
  p.Do(m_platform);
  p.Do(m_blob_type);
  p.Do(m_revision);
  p.Do(m_disc_number);
  m_volume_banner.DoState(p);
  m_emu_state.DoState(p);
  p.Do(m_custom_name);
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    SetWxBannerFromRaw(m_volume_banner);
  }
}

bool GameListItem::IsElfOrDol() const
{
  if (m_file_name.size() < 4)
    return false;

  std::string name_end = m_file_name.substr(m_file_name.size() - 4);
  std::transform(name_end.begin(), name_end.end(), name_end.begin(), ::tolower);
  return name_end == ".elf" || name_end == ".dol";
}

void GameListItem::ReadVolumeBanner(std::vector<u8>* image, const std::vector<u32>& buffer,
                                    int width, int height)
{
  image->resize(width * height * 3);
  for (int i = 0; i < width * height; i++)
  {
    (*image)[i * 3 + 0] = (buffer[i] & 0xFF0000) >> 16;
    (*image)[i * 3 + 1] = (buffer[i] & 0x00FF00) >> 8;
    (*image)[i * 3 + 2] = (buffer[i] & 0x0000FF) >> 0;
  }
}

bool GameListItem::SetWxBannerFromPNGFile(const std::string& path)
{
  if (!File::Exists(path))
    return false;

  wxImage image(StrToWxStr(path), wxBITMAP_TYPE_PNG);
  if (!image.IsOk())
    return false;

  m_banner_wx = image;
  return true;
}

void GameListItem::SetWxBannerFromRaw(const Banner& banner)
{
  if (banner.empty())
    return;

  // Need to make explicit copy as wxImage uses reference counting for copies combined with only
  // taking a pointer, not the content, when given a buffer to its constructor.
  m_banner_wx.Create(banner.width, banner.height, false);
  std::memcpy(m_banner_wx.GetData(), banner.buffer.data(), banner.buffer.size());
}

bool GameListItem::BannerChanged()
{
  // Wii banners can only be read if there is a savefile,
  // so sometimes caches don't contain banners. Let's check
  // if a banner has become available after the cache was made.

  if (!m_volume_banner.empty())
    return false;
  if (!DiscIO::IsWii(m_platform))
    return false;

  auto& banner = m_pending.volume_banner;
  std::vector<u32> buffer = DiscIO::Volume::GetWiiBanner(&banner.width, &banner.height, m_title_id);
  if (!buffer.size())
    return false;

  ReadVolumeBanner(&banner.buffer, buffer, banner.width, banner.height);
  // We only reach here if m_volume_banner was empty, so we don't need to explicitly
  // compare to see if they are different
  return true;
}

void GameListItem::BannerCommit()
{
  m_volume_banner = std::move(m_pending.volume_banner);
  SetWxBannerFromRaw(m_volume_banner);
}

std::string GameListItem::GetDescription(DiscIO::Language language) const
{
  return GetLanguageString(language, m_descriptions);
}

std::string GameListItem::GetDescription() const
{
  const bool wii = DiscIO::IsWii(m_platform);
  return GetDescription(SConfig::GetInstance().GetCurrentLanguage(wii));
}

std::string GameListItem::GetName(DiscIO::Language language) const
{
  return GetLanguageString(language, m_names);
}

std::string GameListItem::GetName() const
{
  if (!m_custom_name.empty())
    return m_custom_name;

  const bool wii = DiscIO::IsWii(m_platform);
  std::string name = GetName(SConfig::GetInstance().GetCurrentLanguage(wii));
  if (!name.empty())
    return name;

  // No usable name, return filename (better than nothing)
  std::string ext;
  SplitPath(GetFileName(), nullptr, &name, &ext);
  return name + ext;
}

std::string GameListItem::GetUniqueIdentifier() const
{
  const DiscIO::Language lang = DiscIO::Language::LANGUAGE_ENGLISH;
  std::vector<std::string> info;
  if (!GetGameID().empty())
    info.push_back(GetGameID());
  if (GetRevision() != 0)
  {
    std::string rev_str = "Revision ";
    info.push_back(rev_str + std::to_string((long long)GetRevision()));
  }

  std::string name(GetName(lang));
  if (name.empty())
    name = GetName();

  int disc_number = GetDiscNumber() + 1;

  std::string lower_name = name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
  if (disc_number > 1 &&
      lower_name.find(std::string(wxString::Format("disc %i", disc_number))) == std::string::npos &&
      lower_name.find(std::string(wxString::Format("disc%i", disc_number))) == std::string::npos)
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

std::vector<DiscIO::Language> GameListItem::GetLanguages() const
{
  std::vector<DiscIO::Language> languages;
  for (std::pair<DiscIO::Language, std::string> name : m_names)
    languages.push_back(name.first);
  return languages;
}

const std::string GameListItem::GetWiiFSPath() const
{
  if (!DiscIO::IsWii(m_platform))
    return "";

  const std::string path = Common::GetTitleDataPath(m_title_id, Common::FROM_CONFIGURED_ROOT);

  if (path[0] == '.')
    return WxStrToStr(wxGetCwd()) + path.substr(strlen(ROOT_DIR));

  return path;
}
