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
#include <tuple>
#include <utility>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/Image.h"
#include "Common/IniFile.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

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

bool operator==(const GameBanner& lhs, const GameBanner& rhs)
{
  return std::tie(lhs.buffer, lhs.width, lhs.height) == std::tie(rhs.buffer, rhs.width, rhs.height);
}

bool operator!=(const GameBanner& lhs, const GameBanner& rhs)
{
  return !operator==(lhs, rhs);
}

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
  m_custom_banner.DoState(p);
}

bool GameFile::IsElfOrDol() const
{
  if (m_file_path.size() < 4)
    return false;

  std::string name_end = m_file_path.substr(m_file_path.size() - 4);
  std::transform(name_end.begin(), name_end.end(), name_end.begin(), ::tolower);
  return name_end == ".elf" || name_end == ".dol";
}

bool GameFile::WiiBannerChanged()
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

void GameFile::WiiBannerCommit()
{
  m_volume_banner = std::move(m_pending.volume_banner);
}

bool GameFile::ReadPNGBanner(const std::string& path)
{
  File::IOFile file(path, "rb");
  if (!file)
    return false;

  std::vector<u8> png_data(file.GetSize());
  if (!file.ReadBytes(png_data.data(), png_data.size()))
    return false;

  GameBanner& banner = m_pending.custom_banner;
  std::vector<u8> data_out;
  if (!Common::LoadPNG(png_data, &data_out, &banner.width, &banner.height))
    return false;

  // Make an ARGB copy of the RGBA data
  banner.buffer.resize(data_out.size() / sizeof(u32));
  for (size_t i = 0; i < banner.buffer.size(); i++)
  {
    const size_t j = i * sizeof(u32);
    banner.buffer[i] = (Common::swap32(data_out.data() + j) >> 8) + (data_out[j] << 24);
  }

  return true;
}

bool GameFile::CustomBannerChanged()
{
  std::string path, name;
  SplitPath(m_file_path, &path, &name, nullptr);

  // This icon naming format is intended as an alternative to Homebrew Channel icons
  // for those who don't want to have a Homebrew Channel style folder structure.
  if (!ReadPNGBanner(path + name + ".png"))
  {
    // Homebrew Channel icon naming. Typical for DOLs and ELFs, but we also support it for volumes.
    if (!ReadPNGBanner(path + "icon.png"))
    {
      // If no custom icon is found, go back to the non-custom one.
      m_pending.custom_banner = {};
    }
  }

  return m_pending.custom_banner != m_custom_banner;
}

void GameFile::CustomBannerCommit()
{
  m_custom_banner = std::move(m_pending.custom_banner);
}

const std::string& GameFile::GetName(const Core::TitleDatabase& title_database) const
{
  const auto type = m_platform == DiscIO::Platform::WiiWAD ?
                        Core::TitleDatabase::TitleType::Channel :
                        Core::TitleDatabase::TitleType::Other;
  const std::string& custom_name = title_database.GetTitleName(m_game_id, type);
  return custom_name.empty() ? GetName() : custom_name;
}

const std::string& GameFile::GetName(bool long_name) const
{
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

  const std::string& name = GetName();

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

const GameBanner& GameFile::GetBannerImage() const
{
  return m_custom_banner.empty() ? m_volume_banner : m_custom_banner;
}

}  // namespace UICommon
