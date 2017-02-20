// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common/Common.h"

#include <wx/bitmap.h>
#include <wx/image.h>

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
  GameListItem(const std::string& _rFileName,
               const std::unordered_map<std::string, std::string>& custom_titles);
  ~GameListItem();

  // Reload settings after INI changes
  void ReloadINI();

  bool IsValid() const { return m_Valid; }
  const std::string& GetFileName() const { return m_FileName; }
  std::string GetName(DiscIO::Language language) const;
  std::string GetName() const;
  std::string GetUniqueIdentifier() const;
  std::string GetDescription(DiscIO::Language language) const;
  std::string GetDescription() const;
  std::vector<DiscIO::Language> GetLanguages() const;
  std::string GetCompany() const { return m_company; }
  u16 GetRevision() const { return m_Revision; }
  const std::string& GetGameID() const { return m_game_id; }
  const std::string GetWiiFSPath() const;
  DiscIO::Region GetRegion() const { return m_region; }
  DiscIO::Country GetCountry() const { return m_Country; }
  DiscIO::Platform GetPlatform() const { return m_Platform; }
  DiscIO::BlobType GetBlobType() const { return m_blob_type; }
  const std::string& GetIssues() const { return m_issues; }
  int GetEmuState() const { return m_emu_state; }
  u64 GetFileSize() const { return m_FileSize; }
  u64 GetVolumeSize() const { return m_VolumeSize; }
  // 0 is the first disc, 1 is the second disc
  u8 GetDiscNumber() const { return m_disc_number; }
  // NOTE: Banner image is at the original resolution, use WxUtils::ScaleImageToBitmap
  //   to display it
  const wxImage& GetBannerImage() const { return m_image; }
  void DoState(PointerWrap& p);

private:
  std::string m_FileName;

  std::map<DiscIO::Language, std::string> m_names;
  std::map<DiscIO::Language, std::string> m_descriptions;
  std::string m_company;

  std::string m_game_id;
  u64 m_title_id;

  std::string m_issues;
  int m_emu_state;

  u64 m_FileSize;
  u64 m_VolumeSize;

  DiscIO::Region m_region;
  DiscIO::Country m_Country;
  DiscIO::Platform m_Platform;
  DiscIO::BlobType m_blob_type;
  u16 m_Revision;

  wxImage m_image;
  bool m_Valid;
  std::vector<u8> m_pImage;
  int m_ImageWidth, m_ImageHeight;
  u8 m_disc_number;

  std::string m_custom_name_titles_txt;  // Custom title from titles.txt
  std::string m_custom_name;             // Custom title from INI or titles.txt
  bool m_has_custom_name;

  bool LoadFromCache();
  void SaveToCache();

  bool IsElfOrDol() const;
  std::string CreateCacheFilename() const;

  // Outputs to m_pImage
  void ReadVolumeBanner(const std::vector<u32>& buffer, int width, int height);
  // Outputs to m_Bitmap
  bool ReadPNGBanner(const std::string& path);
};
