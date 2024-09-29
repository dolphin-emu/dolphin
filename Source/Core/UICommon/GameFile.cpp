// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/GameFile.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <pugixml.hpp>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/IOFile.h"
#include "Common/Image.h"
#include "Common/IniFile.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "Core/Config/UISettings.h"
#include "Core/ConfigManager.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/TitleDatabase.h"

#include "DiscIO/Blob.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Enums.h"
#include "DiscIO/GameModDescriptor.h"
#include "DiscIO/Volume.h"
#include "DiscIO/WiiSaveBanner.h"

namespace UICommon
{
namespace
{
const std::string EMPTY_STRING;

bool UseGameCovers()
{
#ifdef ANDROID
  // Android has its own code for handling covers, written completely in Java.
  // It's best if we disable the C++ cover code on Android to avoid duplicated data and such.
  return false;
#else
  return Config::Get(Config::MAIN_USE_GAME_COVERS);
#endif
}
}  // Anonymous namespace

DiscIO::Language GameFile::GetConfigLanguage() const
{
  return SConfig::GetInstance().GetLanguageAdjustedForRegion(DiscIO::IsWii(m_platform), m_region);
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
  return Lookup(GetConfigLanguage(), strings);
}

GameFile::GameFile() = default;

GameFile::GameFile(std::string path) : m_file_path(std::move(path))
{
  m_file_name = PathToFileName(m_file_path);

  {
    std::unique_ptr<DiscIO::Volume> volume(DiscIO::CreateVolume(m_file_path));
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
      m_block_size = volume->GetBlobReader().GetBlockSize();
      m_compression_method = volume->GetBlobReader().GetCompressionMethod();
      m_file_size = volume->GetRawSize();
      m_volume_size = volume->GetDataSize();
      m_volume_size_type = volume->GetDataSizeType();
      m_is_datel_disc = volume->IsDatelDisc();
      m_is_nkit = volume->IsNKit();

      m_internal_name = volume->GetInternalName();
      m_game_id = volume->GetGameID();
      m_gametdb_id = volume->GetGameTDBID();
      m_title_id = volume->GetTitleID().value_or(0);
      m_maker_id = volume->GetMakerID();
      m_revision = volume->GetRevision().value_or(0);
      m_disc_number = volume->GetDiscNumber().value_or(0);
      m_is_two_disc_game = CheckIfTwoDiscGame(m_game_id);
      m_apploader_date = volume->GetApploaderDate();

      m_volume_banner.buffer = volume->GetBanner(&m_volume_banner.width, &m_volume_banner.height);

      m_valid = true;
    }
  }

  if (!IsValid() && IsElfOrDol())
  {
    m_valid = true;
    m_file_size = m_volume_size = File::GetSize(m_file_path);
    m_game_id = SConfig::MakeGameID(m_file_name);
    m_volume_size_type = DiscIO::DataSizeType::Accurate;
    m_is_datel_disc = false;
    m_is_nkit = false;
    m_platform = DiscIO::Platform::ELFOrDOL;
    m_blob_type = DiscIO::BlobType::DIRECTORY;
  }

  if (!IsValid() && GetExtension() == ".json")
  {
    auto descriptor = DiscIO::ParseGameModDescriptorFile(m_file_path);
    if (descriptor)
    {
      GameFile proxy(descriptor->base_file);
      if (proxy.IsValid())
      {
        m_valid = true;
        m_file_size = File::GetSize(m_file_path);
        m_long_names.emplace(DiscIO::Language::English, std::move(descriptor->display_name));
        if (!descriptor->maker.empty())
          m_long_makers.emplace(DiscIO::Language::English, std::move(descriptor->maker));
        m_internal_name = proxy.GetInternalName();
        m_game_id = proxy.GetGameID();
        m_gametdb_id = proxy.GetGameTDBID();
        m_title_id = proxy.GetTitleID();
        m_maker_id = proxy.GetMakerID();
        m_region = proxy.GetRegion();
        m_country = proxy.GetCountry();
        m_platform = proxy.GetPlatform();
        m_revision = proxy.GetRevision();
        m_disc_number = proxy.GetDiscNumber();
        m_blob_type = DiscIO::BlobType::MOD_DESCRIPTOR;
      }
    }
  }
}

GameFile::~GameFile() = default;

bool GameFile::IsValid() const
{
  if (!m_valid)
    return false;

  if (m_platform == DiscIO::Platform::WiiWAD && !IOS::ES::IsChannel(m_title_id))
    return false;

  return true;
}

bool GameFile::CustomCoverChanged()
{
  if (!m_custom_cover.buffer.empty() || !UseGameCovers())
    return false;

  std::string path, name;
  SplitPath(m_file_path, &path, &name, nullptr);

  std::string contents;

  // This icon naming format is intended as an alternative to Homebrew Channel icons
  // for those who don't want to have a Homebrew Channel style folder structure.
  const std::string cover_path = path + name + ".cover.png";
  bool success = File::Exists(cover_path) && File::ReadFileToString(cover_path, contents);

  if (!success)
  {
    const std::string alt_cover_path = path + "cover.png";
    success = File::Exists(alt_cover_path) && File::ReadFileToString(alt_cover_path, contents);
  }

  if (success)
    m_pending.custom_cover.buffer = {contents.begin(), contents.end()};

  return success;
}

void GameFile::DownloadDefaultCover()
{
  if (!m_default_cover.buffer.empty() || !UseGameCovers() || m_gametdb_id.empty())
    return;

  const auto cover_path = File::GetUserPath(D_COVERCACHE_IDX) + DIR_SEP;
  const auto png_path = cover_path + m_gametdb_id + ".png";

  // If the cover has already been downloaded, abort
  if (File::Exists(png_path))
    return;

  const std::string region_code =
      SConfig::GetInstance().GetGameTDBImageRegionCode(DiscIO::IsWii(GetPlatform()), m_region);

  Common::HttpRequest request;
  static constexpr char cover_url[] = "https://art.gametdb.com/wii/cover/{}/{}.png";
  const auto response = request.Get(fmt::format(cover_url, region_code, m_gametdb_id));

  if (!response)
    return;

  File::WriteStringToFile(png_path, std::string(response->begin(), response->end()));
}

bool GameFile::DefaultCoverChanged()
{
  if (!m_default_cover.buffer.empty() || !UseGameCovers())
    return false;

  const auto cover_path = File::GetUserPath(D_COVERCACHE_IDX) + DIR_SEP;

  std::string contents;

  File::ReadFileToString(cover_path + m_gametdb_id + ".png", contents);

  if (contents.empty())
    return false;

  m_pending.default_cover.buffer = {contents.begin(), contents.end()};

  return true;
}

void GameFile::CustomCoverCommit()
{
  m_custom_cover = std::move(m_pending.custom_cover);
}

void GameFile::DefaultCoverCommit()
{
  m_default_cover = std::move(m_pending.default_cover);
}

void GameBanner::DoState(PointerWrap& p)
{
  p.Do(buffer);
  p.Do(width);
  p.Do(height);
}

void GameCover::DoState(PointerWrap& p)
{
  p.Do(buffer);
}

void GameFile::DoState(PointerWrap& p)
{
  p.Do(m_valid);
  p.Do(m_file_path);
  p.Do(m_file_name);

  p.Do(m_file_size);
  p.Do(m_volume_size);
  p.Do(m_volume_size_type);
  p.Do(m_is_datel_disc);
  p.Do(m_is_nkit);

  p.Do(m_short_names);
  p.Do(m_long_names);
  p.Do(m_short_makers);
  p.Do(m_long_makers);
  p.Do(m_descriptions);
  p.Do(m_internal_name);
  p.Do(m_game_id);
  p.Do(m_gametdb_id);
  p.Do(m_title_id);
  p.Do(m_maker_id);

  p.Do(m_region);
  p.Do(m_country);
  p.Do(m_platform);
  p.Do(m_blob_type);
  p.Do(m_block_size);
  p.Do(m_compression_method);
  p.Do(m_revision);
  p.Do(m_disc_number);
  p.Do(m_is_two_disc_game);
  p.Do(m_apploader_date);

  p.Do(m_custom_name);
  p.Do(m_custom_description);
  p.Do(m_custom_maker);
  m_volume_banner.DoState(p);
  m_custom_banner.DoState(p);
  m_default_cover.DoState(p);
  m_custom_cover.DoState(p);
}

std::string GameFile::GetExtension() const
{
  std::string extension;
  SplitPath(m_file_path, nullptr, nullptr, &extension);
  Common::ToLower(&extension);
  return extension;
}

bool GameFile::IsElfOrDol() const
{
  const std::string extension = GetExtension();
  return extension == ".elf" || extension == ".dol";
}

bool GameFile::ReadXMLMetadata(const std::string& path)
{
  std::string data;
  if (!File::ReadFileToString(path, data))
    return false;

  pugi::xml_document doc;
  // We use load_buffer instead of load_file to avoid path encoding problems on Windows
  if (!doc.load_buffer(data.data(), data.size()))
    return false;

  const pugi::xml_node app_node = doc.child("app");
  m_pending.custom_name = app_node.child("name").text().as_string();
  m_pending.custom_maker = app_node.child("coder").text().as_string();
  m_pending.custom_description = app_node.child("short_description").text().as_string();

  // Elements that we aren't using:
  // version (can be written in any format)
  // release_date (YYYYmmddHHMMSS format)
  // long_description (can be several screens long!)

  return true;
}

bool GameFile::XMLMetadataChanged()
{
  std::string path, name;
  SplitPath(m_file_path, &path, &name, nullptr);

  // This XML file naming format is intended as an alternative to the Homebrew Channel naming
  // for those who don't want to have a Homebrew Channel style folder structure.
  if (!ReadXMLMetadata(path + name + ".xml"))
  {
    // Homebrew Channel naming. Typical for DOLs and ELFs, but we also support it for volumes.
    if (!ReadXMLMetadata(path + "meta.xml"))
    {
      // If no XML metadata is found, remove any old XML metadata from memory.
      m_pending.custom_banner = {};
    }
  }

  return m_pending.custom_name != m_custom_name && m_pending.custom_maker != m_custom_maker &&
         m_pending.custom_description != m_custom_description;
}

void GameFile::XMLMetadataCommit()
{
  m_custom_name = std::move(m_pending.custom_name);
  m_custom_description = std::move(m_pending.custom_description);
  m_custom_maker = std::move(m_pending.custom_maker);
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

bool GameFile::TryLoadGameModDescriptorBanner()
{
  if (m_blob_type != DiscIO::BlobType::MOD_DESCRIPTOR)
    return false;

  auto descriptor = DiscIO::ParseGameModDescriptorFile(m_file_path);
  if (!descriptor)
    return false;

  return ReadPNGBanner(descriptor->banner);
}

bool GameFile::CustomBannerChanged()
{
  std::string path, name;
  SplitPath(m_file_path, &path, &name, nullptr);

  // This icon naming format is intended as an alternative to the Homebrew Channel naming
  // for those who don't want to have a Homebrew Channel style folder structure.
  if (!ReadPNGBanner(path + name + ".png"))
  {
    // Homebrew Channel icon naming. Typical for DOLs and ELFs, but we also support it for volumes.
    if (!ReadPNGBanner(path + "icon.png"))
    {
      // If it's a game mod descriptor file, it may specify its own custom banner.
      if (!TryLoadGameModDescriptorBanner())
      {
        // If no custom icon is found, go back to the non-custom one.
        m_pending.custom_banner = {};
      }
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
  if (!m_custom_name.empty())
    return m_custom_name;
  if (IsModDescriptor())
    return GetName(Variant::LongAndPossiblyCustom);

  const std::string& database_name = title_database.GetTitleName(m_gametdb_id, GetConfigLanguage());
  return database_name.empty() ? GetName(Variant::LongAndPossiblyCustom) : database_name;
}

const std::string& GameFile::GetName(Variant variant) const
{
  if (variant == Variant::LongAndPossiblyCustom && !m_custom_name.empty())
    return m_custom_name;

  const std::string& name = variant == Variant::ShortAndNotCustom ? GetShortName() : GetLongName();
  if (!name.empty())
    return name;

  // No usable name, return filename (better than nothing)
  return m_file_name;
}

const std::string& GameFile::GetMaker(Variant variant) const
{
  if (variant == Variant::LongAndPossiblyCustom && !m_custom_maker.empty())
    return m_custom_maker;

  const std::string& maker =
      variant == Variant::ShortAndNotCustom ? GetShortMaker() : GetLongMaker();
  if (!maker.empty())
    return maker;

  if (m_game_id.size() >= 6)
    return DiscIO::GetCompanyFromID(m_maker_id);

  return EMPTY_STRING;
}

const std::string& GameFile::GetDescription(Variant variant) const
{
  if (variant == Variant::LongAndPossiblyCustom && !m_custom_description.empty())
    return m_custom_description;

  return LookupUsingConfigLanguage(m_descriptions);
}

std::vector<DiscIO::Language> GameFile::GetLanguages() const
{
  std::vector<DiscIO::Language> languages;
  // TODO: What if some languages don't have long names but have other strings?
  for (const auto& name : m_long_names)
    languages.push_back(name.first);
  return languages;
}

bool GameFile::CheckIfTwoDiscGame(const std::string& game_id) const
{
  constexpr size_t GAME_ID_PREFIX_SIZE = 3;
  if (game_id.size() < GAME_ID_PREFIX_SIZE)
    return false;

  static constexpr std::array<std::string_view, 30> two_disc_game_id_prefixes = {
      // Resident Evil
      "DBJ",
      // The Lord of the Rings: The Third Age
      "G3A",
      // Teenage Mutant Ninja Turtles 3: Mutant Nightmare
      "G3Q",
      // Resident Evil 4
      "G4B",
      // Tiger Woods PGA Tour 2005
      "G5T",
      // Resident Evil
      "GBI",
      // Resident Evil Zero
      "GBZ",
      // Conan
      "GC9",
      // Resident Evil Code: Veronica X
      "GCD",
      // Tom Clancy's Splinter Cell: Chaos Theory
      "GCJ",
      // Freaky Flyers
      "GFF",
      // GoldenEye: Rogue Agent
      "GGI",
      // Metal Gear Solid: The Twin Snakes
      "GGS",
      // Baten Kaitos Origins
      "GK4",
      // Killer7
      "GK7",
      // Baten Kaitos: Eternal Wings and the Lost Ocean
      "GKB",
      // Lupin the 3rd: Lost Treasure by the Sea
      "GL3",
      // Enter the Matrix
      "GMX",
      // Teenage Mutant Ninja Turtles 2: Battle Nexus
      "GNI",
      // GoldenEye: Rogue Agent
      "GOY",
      // Tales of Symphonia
      "GQS",
      // Medal of Honor: Rising Sun
      "GR8",
      "GRZ",
      // Tales of Symphonia
      "GTO",
      // Tiger Woods PGA Tour 2004
      "GW4",
      // Tom Clancy's Splinter Cell: Double Agent (GC)
      "GWY",
      // Dragon Quest X: Mezameshi Itsutsu no Shuzoku Online
      "S4M",
      "S4S",
      "S6T",
      "SDQ",
  };
  static_assert(std::ranges::is_sorted(two_disc_game_id_prefixes));

  std::string_view game_id_prefix(game_id.data(), GAME_ID_PREFIX_SIZE);
  return std::ranges::binary_search(two_disc_game_id_prefixes, game_id_prefix);
}

std::string GameFile::GetNetPlayName(const Core::TitleDatabase& title_database) const
{
  std::vector<std::string> info;
  if (!GetGameID().empty())
    info.push_back(GetGameID());
  if (GetRevision() != 0)
    info.push_back("Revision " + std::to_string(GetRevision()));

  const std::string name = GetName(title_database);

  int disc_number = GetDiscNumber() + 1;

  std::string lower_name = name;
  Common::ToLower(&lower_name);
  if (disc_number > 1 &&
      lower_name.find(fmt::format("disc {}", disc_number)) == std::string::npos &&
      lower_name.find(fmt::format("disc{}", disc_number)) == std::string::npos)
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

static Common::SHA1::Digest GetHash(u32 value)
{
  auto data = Common::BitCastToArray<u8>(value);
  return Common::SHA1::CalculateDigest(data);
}

static Common::SHA1::Digest GetHash(std::string_view str)
{
  return Common::SHA1::CalculateDigest(str);
}

static std::optional<Common::SHA1::Digest> GetFileHash(const std::string& path)
{
  std::string buffer;
  if (!File::ReadFileToString(path, buffer))
    return std::nullopt;
  return GetHash(buffer);
}

static std::optional<Common::SHA1::Digest> MixHash(const std::optional<Common::SHA1::Digest>& lhs,
                                                   const std::optional<Common::SHA1::Digest>& rhs)
{
  if (!lhs && !rhs)
    return std::nullopt;
  if (!lhs || !rhs)
    return !rhs ? lhs : rhs;
  Common::SHA1::Digest result;
  for (size_t i = 0; i < result.size(); ++i)
    result[i] = (*lhs)[i] ^ (*rhs)[(i + 1) % result.size()];
  return result;
}

Common::SHA1::Digest GameFile::GetSyncHash() const
{
  std::optional<Common::SHA1::Digest> hash;

  if (m_platform == DiscIO::Platform::ELFOrDOL)
  {
    hash = GetFileHash(m_file_path);
  }
  else if (m_blob_type == DiscIO::BlobType::MOD_DESCRIPTOR)
  {
    auto descriptor = DiscIO::ParseGameModDescriptorFile(m_file_path);
    if (descriptor)
    {
      GameFile proxy(descriptor->base_file);
      if (proxy.IsValid())
        hash = proxy.GetSyncHash();

      // add patches to hash if they're enabled
      if (descriptor->riivolution)
      {
        for (const auto& patch : descriptor->riivolution->patches)
        {
          hash = MixHash(hash, GetFileHash(patch.xml));
          for (const auto& option : patch.options)
          {
            hash = MixHash(hash, GetHash(option.section_name));
            hash = MixHash(hash, GetHash(option.option_id));
            hash = MixHash(hash, GetHash(option.option_name));
            hash = MixHash(hash, GetHash(option.choice));
          }
        }
      }
    }
  }
  else
  {
    if (std::unique_ptr<DiscIO::Volume> volume = DiscIO::CreateVolume(m_file_path))
      hash = volume->GetSyncHash();
  }

  return hash.value_or(Common::SHA1::Digest{});
}

NetPlay::SyncIdentifier GameFile::GetSyncIdentifier() const
{
  const u64 dol_elf_size = m_platform == DiscIO::Platform::ELFOrDOL ? m_file_size : 0;
  return NetPlay::SyncIdentifier{dol_elf_size,  m_game_id,       m_revision,
                                 m_disc_number, m_is_datel_disc, GetSyncHash()};
}

NetPlay::SyncIdentifierComparison
GameFile::CompareSyncIdentifier(const NetPlay::SyncIdentifier& sync_identifier) const
{
  const bool is_elf_or_dol = m_platform == DiscIO::Platform::ELFOrDOL;
  if ((is_elf_or_dol ? m_file_size : 0) != sync_identifier.dol_elf_size)
    return NetPlay::SyncIdentifierComparison::DifferentGame;

  if (m_is_datel_disc != sync_identifier.is_datel)
    return NetPlay::SyncIdentifierComparison::DifferentGame;

  if (m_game_id.size() != sync_identifier.game_id.size())
    return NetPlay::SyncIdentifierComparison::DifferentGame;

  if (!is_elf_or_dol && !m_is_datel_disc && m_game_id.size() >= 4 && m_game_id.size() <= 6)
  {
    // This is a game ID, following specific rules which we can use to give clearer information to
    // the user.

    // If the first 3 characters don't match, then these are probably different games.
    // (There are exceptions; in particular Japanese-region games sometimes use a different ID;
    // for instance, GOYE69, GOYP69, and GGIJ13 are used by GoldenEye: Rogue Agent.)
    if (std::string_view(&m_game_id[0], 3) != std::string_view(&sync_identifier.game_id[0], 3))
      return NetPlay::SyncIdentifierComparison::DifferentGame;

    // If the first 3 characters match but the region doesn't match, reject it as such.
    if (m_game_id[3] != sync_identifier.game_id[3])
      return NetPlay::SyncIdentifierComparison::DifferentRegion;

    // If the maker code is present, and doesn't match between the two but the main ID does,
    // these might be different revisions of the same game (e.g. a rerelease with a different
    // publisher), which we should treat as a different revision.
    if (std::string_view(&m_game_id[4]) != std::string_view(&sync_identifier.game_id[4]))
      return NetPlay::SyncIdentifierComparison::DifferentRevision;
  }
  else
  {
    // Numeric title ID or another generated ID that does not follow the rules above
    if (m_game_id != sync_identifier.game_id)
      return NetPlay::SyncIdentifierComparison::DifferentGame;
  }

  if (m_revision != sync_identifier.revision)
    return NetPlay::SyncIdentifierComparison::DifferentRevision;

  if (m_disc_number != sync_identifier.disc_number)
    return NetPlay::SyncIdentifierComparison::DifferentDiscNumber;

  if (GetSyncHash() != sync_identifier.sync_hash)
  {
    // Most Datel titles re-use the same game ID (GNHE5d, with that lowercase D, or DTLX01).
    // So if the hash differs, then it's probably a different game even if the game ID matches.
    if (m_is_datel_disc)
      return NetPlay::SyncIdentifierComparison::DifferentGame;
    else
      return NetPlay::SyncIdentifierComparison::DifferentHash;
  }

  return NetPlay::SyncIdentifierComparison::SameGame;
}

std::string GameFile::GetWiiFSPath() const
{
  ASSERT(DiscIO::IsWii(m_platform));
  return Common::GetTitleDataPath(m_title_id, Common::FromWhichRoot::Configured);
}

bool GameFile::ShouldShowFileFormatDetails() const
{
  switch (m_blob_type)
  {
  case DiscIO::BlobType::PLAIN:
    break;
  case DiscIO::BlobType::DRIVE:
  case DiscIO::BlobType::MOD_DESCRIPTOR:
    return false;
  default:
    return true;
  }

  switch (m_platform)
  {
  case DiscIO::Platform::WiiWAD:
    return false;
  case DiscIO::Platform::ELFOrDOL:
    return false;
  default:
    return true;
  }
}

std::string GameFile::GetFileFormatName() const
{
  switch (m_platform)
  {
  case DiscIO::Platform::WiiWAD:
    return "WAD";

  case DiscIO::Platform::ELFOrDOL:
  {
    std::string extension = GetExtension();
    Common::ToUpper(&extension);

    // substr removes the dot
    return extension.substr(std::min<size_t>(1, extension.size()));
  }

  default:
  {
    std::string name = DiscIO::GetName(m_blob_type, true);
    if (m_is_nkit)
      name = Common::FmtFormatT("{0} (NKit)", name);
    return name;
  }
  }
}

bool GameFile::ShouldAllowConversion() const
{
  return DiscIO::IsDisc(m_platform) && m_volume_size_type == DiscIO::DataSizeType::Accurate;
}

bool GameFile::IsModDescriptor() const
{
  return m_blob_type == DiscIO::BlobType::MOD_DESCRIPTOR;
}

const GameBanner& GameFile::GetBannerImage() const
{
  return m_custom_banner.empty() ? m_volume_banner : m_custom_banner;
}

const GameCover& GameFile::GetCoverImage() const
{
  return m_custom_cover.empty() ? m_default_cover : m_custom_cover;
}

}  // namespace UICommon
