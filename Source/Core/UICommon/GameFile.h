// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/SyncIdentifier.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"

class PointerWrap;

namespace Core
{
class TitleDatabase;
}

namespace UICommon
{
struct GameBanner
{
  std::vector<u32> buffer;
  u32 width{};
  u32 height{};

  bool operator==(const GameBanner&) const = default;

  bool empty() const { return buffer.empty(); }
  void DoState(PointerWrap& p);
};

struct GameCover
{
  std::vector<u8> buffer;
  bool empty() const { return buffer.empty(); }
  void DoState(PointerWrap& p);
};

// This class caches the metadata of a DiscIO::Volume (or a DOL/ELF file).
class GameFile final
{
public:
  enum class Variant
  {
    LongAndPossiblyCustom,
    LongAndNotCustom,
    ShortAndNotCustom,
  };

  GameFile();
  explicit GameFile(std::string path);
  ~GameFile();

  bool IsValid() const;
  const std::string& GetFilePath() const { return m_file_path; }
  const std::string& GetFileName() const { return m_file_name; }
  const std::string& GetName(const Core::TitleDatabase& title_database) const;
  const std::string& GetName(Variant variant) const;
  const std::string& GetMaker(Variant variant) const;
  const std::string& GetShortName(DiscIO::Language l) const { return Lookup(l, m_short_names); }
  const std::string& GetShortName() const { return LookupUsingConfigLanguage(m_short_names); }
  const std::string& GetLongName(DiscIO::Language l) const { return Lookup(l, m_long_names); }
  const std::string& GetLongName() const { return LookupUsingConfigLanguage(m_long_names); }
  const std::string& GetShortMaker(DiscIO::Language l) const { return Lookup(l, m_short_makers); }
  const std::string& GetShortMaker() const { return LookupUsingConfigLanguage(m_short_makers); }
  const std::string& GetLongMaker(DiscIO::Language l) const { return Lookup(l, m_long_makers); }
  const std::string& GetLongMaker() const { return LookupUsingConfigLanguage(m_long_makers); }
  const std::string& GetDescription(DiscIO::Language l) const { return Lookup(l, m_descriptions); }
  const std::string& GetDescription(Variant variant) const;
  std::vector<DiscIO::Language> GetLanguages() const;
  const std::string& GetInternalName() const { return m_internal_name; }
  const std::string& GetGameID() const { return m_game_id; }
  const std::string& GetGameTDBID() const { return m_gametdb_id; }
  u64 GetTitleID() const { return m_title_id; }
  const std::string& GetMakerID() const { return m_maker_id; }
  u16 GetRevision() const { return m_revision; }
  // 0 is the first disc, 1 is the second disc
  u8 GetDiscNumber() const { return m_disc_number; }
  bool IsTwoDiscGame() const { return m_is_two_disc_game; }
  std::string GetNetPlayName(const Core::TitleDatabase& title_database) const;

  // This function is slow
  std::array<u8, 20> GetSyncHash() const;
  // This function is slow
  NetPlay::SyncIdentifier GetSyncIdentifier() const;
  // This function is slow if all of game_id, revision, disc_number, is_datel are identical
  NetPlay::SyncIdentifierComparison
  CompareSyncIdentifier(const NetPlay::SyncIdentifier& sync_identifier) const;

  std::string GetWiiFSPath() const;
  DiscIO::Region GetRegion() const { return m_region; }
  DiscIO::Country GetCountry() const { return m_country; }
  DiscIO::Platform GetPlatform() const { return m_platform; }
  DiscIO::BlobType GetBlobType() const { return m_blob_type; }
  u64 GetBlockSize() const { return m_block_size; }
  const std::string& GetCompressionMethod() const { return m_compression_method; }
  bool ShouldShowFileFormatDetails() const;
  std::string GetFileFormatName() const;
  bool ShouldAllowConversion() const;
  const std::string& GetApploaderDate() const { return m_apploader_date; }
  u64 GetFileSize() const { return m_file_size; }
  u64 GetVolumeSize() const { return m_volume_size; }
  DiscIO::DataSizeType GetVolumeSizeType() const { return m_volume_size_type; }
  bool IsDatelDisc() const { return m_is_datel_disc; }
  bool IsNKit() const { return m_is_nkit; }
  bool IsModDescriptor() const;
  const GameBanner& GetBannerImage() const;
  const GameCover& GetCoverImage() const;
  void DoState(PointerWrap& p);
  bool XMLMetadataChanged();
  void XMLMetadataCommit();
  bool WiiBannerChanged();
  void WiiBannerCommit();
  bool CustomBannerChanged();
  void CustomBannerCommit();
  void DownloadDefaultCover();
  bool DefaultCoverChanged();
  void DefaultCoverCommit();
  bool CustomCoverChanged();
  void CustomCoverCommit();

private:
  DiscIO::Language GetConfigLanguage() const;
  static const std::string& Lookup(DiscIO::Language language,
                                   const std::map<DiscIO::Language, std::string>& strings);
  const std::string&
  LookupUsingConfigLanguage(const std::map<DiscIO::Language, std::string>& strings) const;
  std::string GetExtension() const;
  bool IsElfOrDol() const;
  bool ReadXMLMetadata(const std::string& path);
  bool ReadPNGBanner(const std::string& path);
  bool TryLoadGameModDescriptorBanner();
  bool CheckIfTwoDiscGame(const std::string& game_id) const;

  // IMPORTANT: Nearly all data members must be save/restored in DoState.
  // If anything is changed, make sure DoState handles it properly and
  // CACHE_REVISION in GameFileCache.cpp is incremented.

  bool m_valid{};
  std::string m_file_path;
  std::string m_file_name;

  u64 m_file_size{};
  u64 m_volume_size{};
  DiscIO::DataSizeType m_volume_size_type{};
  bool m_is_datel_disc{};
  bool m_is_nkit{};

  std::map<DiscIO::Language, std::string> m_short_names;
  std::map<DiscIO::Language, std::string> m_long_names;
  std::map<DiscIO::Language, std::string> m_short_makers;
  std::map<DiscIO::Language, std::string> m_long_makers;
  std::map<DiscIO::Language, std::string> m_descriptions;
  std::string m_internal_name;
  std::string m_game_id;
  std::string m_gametdb_id;
  u64 m_title_id{};
  std::string m_maker_id;

  DiscIO::Region m_region{DiscIO::Region::Unknown};
  DiscIO::Country m_country{DiscIO::Country::Unknown};
  DiscIO::Platform m_platform{};
  DiscIO::BlobType m_blob_type{};
  u64 m_block_size{};
  std::string m_compression_method{};
  u16 m_revision{};
  u8 m_disc_number{};
  bool m_is_two_disc_game{};
  std::string m_apploader_date;

  std::string m_custom_name;
  std::string m_custom_description;
  std::string m_custom_maker;
  GameBanner m_volume_banner{};
  GameBanner m_custom_banner{};
  GameCover m_default_cover{};
  GameCover m_custom_cover{};

  // The following data members allow GameFileCache to construct updated versions
  // of GameFiles in a threadsafe way. They should not be handled in DoState.
  struct
  {
    std::string custom_name;
    std::string custom_description;
    std::string custom_maker;
    GameBanner volume_banner;
    GameBanner custom_banner;
    GameCover default_cover;
    GameCover custom_cover;
  } m_pending{};
};

}  // namespace UICommon
