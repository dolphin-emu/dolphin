// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace Core
{
class TitleDatabase;
}

namespace DiscIO
{
enum class BlobType;
enum class Country;
enum class Language;
enum class Region;
enum class Platform;
}

namespace UICommon
{
struct GameBanner
{
  std::vector<u32> buffer{};
  int width{};
  int height{};
  bool empty() const { return buffer.empty(); }
  void DoState(PointerWrap& p);
};

// This class caches the metadata of a DiscIO::Volume (or a DOL/ELF file).
class GameFile final
{
public:
  GameFile() = default;
  explicit GameFile(const std::string& path);
  ~GameFile() = default;

  bool IsValid() const;
  const std::string& GetFilePath() const { return m_file_path; }
  const std::string& GetFileName() const { return m_file_name; }
  const std::string& GetName(bool long_name = true) const;
  const std::string& GetMaker(bool long_maker = true) const;
  const std::string& GetShortName(DiscIO::Language l) const { return Lookup(l, m_short_names); }
  const std::string& GetShortName() const { return LookupUsingConfigLanguage(m_short_names); }
  const std::string& GetLongName(DiscIO::Language l) const { return Lookup(l, m_long_names); }
  const std::string& GetLongName() const { return LookupUsingConfigLanguage(m_long_names); }
  const std::string& GetShortMaker(DiscIO::Language l) const { return Lookup(l, m_short_makers); }
  const std::string& GetShortMaker() const { return LookupUsingConfigLanguage(m_short_makers); }
  const std::string& GetLongMaker(DiscIO::Language l) const { return Lookup(l, m_long_makers); }
  const std::string& GetLongMaker() const { return LookupUsingConfigLanguage(m_long_makers); }
  const std::string& GetDescription(DiscIO::Language l) const { return Lookup(l, m_descriptions); }
  const std::string& GetDescription() const { return LookupUsingConfigLanguage(m_descriptions); }
  std::vector<DiscIO::Language> GetLanguages() const;
  const std::string& GetInternalName() const { return m_internal_name; }
  const std::string& GetGameID() const { return m_game_id; }
  u64 GetTitleID() const { return m_title_id; }
  const std::string& GetMakerID() const { return m_maker_id; }
  u16 GetRevision() const { return m_revision; }
  // 0 is the first disc, 1 is the second disc
  u8 GetDiscNumber() const { return m_disc_number; }
  std::string GetUniqueIdentifier() const;
  std::string GetWiiFSPath() const;
  DiscIO::Region GetRegion() const { return m_region; }
  DiscIO::Country GetCountry() const { return m_country; }
  DiscIO::Platform GetPlatform() const { return m_platform; }
  DiscIO::BlobType GetBlobType() const { return m_blob_type; }
  const std::string& GetApploaderDate() const { return m_apploader_date; }
  const std::string& GetIssues() const { return m_emu_state.issues; }
  int GetEmuState() const { return m_emu_state.rating; }
  u64 GetFileSize() const { return m_file_size; }
  u64 GetVolumeSize() const { return m_volume_size; }
  const GameBanner& GetBannerImage() const { return m_volume_banner; }
  void DoState(PointerWrap& p);
  bool BannerChanged();
  void BannerCommit();
  bool EmuStateChanged();
  void EmuStateCommit();
  bool CustomNameChanged(const Core::TitleDatabase& title_database);
  void CustomNameCommit();

private:
  struct EmuState
  {
    int rating{};
    std::string issues{};
    bool operator!=(const EmuState& rhs) const
    {
      return rating != rhs.rating || issues != rhs.issues;
    }
    void DoState(PointerWrap& p);
  };

  static const std::string& Lookup(DiscIO::Language language,
                                   const std::map<DiscIO::Language, std::string>& strings);
  const std::string&
  LookupUsingConfigLanguage(const std::map<DiscIO::Language, std::string>& strings) const;
  bool IsElfOrDol() const;

  // IMPORTANT: Nearly all data members must be save/restored in DoState.
  // If anything is changed, make sure DoState handles it properly and
  // CACHE_REVISION in GameFileCache.cpp is incremented.

  bool m_valid{};
  std::string m_file_path{};
  std::string m_file_name{};

  u64 m_file_size{};
  u64 m_volume_size{};

  std::map<DiscIO::Language, std::string> m_short_names{};
  std::map<DiscIO::Language, std::string> m_long_names{};
  std::map<DiscIO::Language, std::string> m_short_makers{};
  std::map<DiscIO::Language, std::string> m_long_makers{};
  std::map<DiscIO::Language, std::string> m_descriptions{};
  std::string m_internal_name{};
  std::string m_game_id{};
  u64 m_title_id{};
  std::string m_maker_id{};

  DiscIO::Region m_region{};
  DiscIO::Country m_country{};
  DiscIO::Platform m_platform{};
  DiscIO::BlobType m_blob_type{};
  u16 m_revision{};
  u8 m_disc_number{};
  std::string m_apploader_date{};

  GameBanner m_volume_banner{};
  EmuState m_emu_state{};
  // Overridden name from TitleDatabase
  std::string m_custom_name{};

  // The following data members allow GameFileCache to construct updated versions
  // of GameFiles in a threadsafe way. They should not be handled in DoState.
  struct
  {
    EmuState emu_state;
    GameBanner volume_banner;
    std::string custom_name;
  } m_pending{};
};

}  // namespace UICommon
