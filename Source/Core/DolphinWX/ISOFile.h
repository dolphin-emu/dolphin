// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Common/Common.h"

#include <wx/bitmap.h>
#include <wx/image.h>

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

class PointerWrap;

class GameListItem
{
public:
  GameListItem() = default;
  explicit GameListItem(const std::string& file_name);
  ~GameListItem() = default;

  bool IsValid() const;
  const std::string& GetFileName() const { return m_file_name; }
  std::string GetName(DiscIO::Language language) const;
  std::string GetName() const;
  std::string GetUniqueIdentifier() const;
  std::string GetDescription(DiscIO::Language language) const;
  std::string GetDescription() const;
  std::vector<DiscIO::Language> GetLanguages() const;
  std::string GetCompany() const { return m_company; }
  u16 GetRevision() const { return m_revision; }
  const std::string& GetGameID() const { return m_game_id; }
  u64 GetTitleID() const { return m_title_id; }
  const std::string GetWiiFSPath() const;
  DiscIO::Region GetRegion() const { return m_region; }
  DiscIO::Country GetCountry() const { return m_country; }
  DiscIO::Platform GetPlatform() const { return m_platform; }
  DiscIO::BlobType GetBlobType() const { return m_blob_type; }
  const std::string& GetIssues() const { return m_emu_state.issues; }
  int GetEmuState() const { return m_emu_state.rating; }
  u64 GetFileSize() const { return m_file_size; }
  u64 GetVolumeSize() const { return m_volume_size; }
  // 0 is the first disc, 1 is the second disc
  u8 GetDiscNumber() const { return m_disc_number; }
  // NOTE: Banner image is at the original resolution, use WxUtils::ScaleImageToBitmap
  //   to display it
  const wxImage& GetBannerImage() const { return m_banner_wx; }
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
  struct Banner
  {
    std::vector<u8> buffer{};
    int width{};
    int height{};
    bool empty() const { return buffer.empty(); }
    void DoState(PointerWrap& p);
  };

  bool IsElfOrDol() const;
  void ReadVolumeBanner(std::vector<u8>* image, const std::vector<u32>& buffer, int width,
                        int height);
  // Outputs to m_banner_wx
  bool SetWxBannerFromPNGFile(const std::string& path);
  // Outputs to m_banner_wx
  void SetWxBannerFromRaw(const Banner& banner);

  // IMPORTANT: Nearly all data members must be save/restored in DoState.
  // If anything is changed, make sure DoState handles it properly and
  // GameListCtrl::CACHE_REVISION is incremented.

  bool m_valid{};
  std::string m_file_name{};

  u64 m_file_size{};
  u64 m_volume_size{};

  std::map<DiscIO::Language, std::string> m_names{};
  std::map<DiscIO::Language, std::string> m_descriptions{};
  std::string m_company{};
  std::string m_game_id{};
  u64 m_title_id{};

  DiscIO::Region m_region{};
  DiscIO::Country m_country{};
  DiscIO::Platform m_platform{};
  DiscIO::BlobType m_blob_type{};
  u16 m_revision{};
  u8 m_disc_number{};

  Banner m_volume_banner{};
  EmuState m_emu_state{};
  // Overridden name from TitleDatabase
  std::string m_custom_name{};

  // wxImage is not handled in DoState
  wxImage m_banner_wx{};

  // The following data members allow GameListCtrl to construct new GameListItems in a threadsafe
  // way. They should not be handled in DoState.
  struct
  {
    EmuState emu_state;
    Banner volume_banner;
    std::string custom_name;
  } m_pending{};
};
