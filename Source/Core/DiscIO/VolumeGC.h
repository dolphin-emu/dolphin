// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume.h"

// --- this volume type is used for GC disc images ---

namespace DiscIO
{
class IBlobReader;
enum class BlobType;
enum class Country;
enum class Language;
enum class Region;
enum class Platform;

class CVolumeGC : public IVolume
{
public:
  CVolumeGC(std::unique_ptr<IBlobReader> reader);
  ~CVolumeGC();
  bool Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt = false) const override;
  std::string GetGameID() const override;
  std::string GetMakerID() const override;
  u16 GetRevision() const override;
  std::string GetInternalName() const override;
  std::map<Language, std::string> GetShortNames() const override;
  std::map<Language, std::string> GetLongNames() const override;
  std::map<Language, std::string> GetShortMakers() const override;
  std::map<Language, std::string> GetLongMakers() const override;
  std::map<Language, std::string> GetDescriptions() const override;
  std::vector<u32> GetBanner(int* width, int* height) const override;
  u64 GetFSTSize() const override;
  std::string GetApploaderDate() const override;
  u8 GetDiscNumber() const override;

  Platform GetVolumeType() const override;
  Region GetRegion() const override;
  Country GetCountry() const override;
  BlobType GetBlobType() const override;
  u64 GetSize() const override;
  u64 GetRawSize() const override;

private:
  static const int GC_BANNER_WIDTH = 96;
  static const int GC_BANNER_HEIGHT = 32;

  struct GCBannerInformation
  {
    char short_name[32];    // Short game title shown in IPL menu
    char short_maker[32];   // Short developer, publisher names shown in IPL menu
    char long_name[64];     // Long game title shown in IPL game start screen
    char long_maker[64];    // Long developer, publisher names shown in IPL game
                            // start screen
    char description[128];  // Game description shown in IPL game start screen in
                            // two lines.
  };

  struct GCBanner
  {
    u32 id;  // "BNR1" for NTSC, "BNR2" for PAL
    u32 padding[7];
    u16 image[GC_BANNER_WIDTH * GC_BANNER_HEIGHT];  // RGB5A3 96x32 image
    GCBannerInformation information[6];             // information comes in six languages
                                                    // (only one for BNR1 type)
  };

  void LoadBannerFile() const;
  void ExtractBannerInformation(const GCBanner& banner_file, bool is_bnr1) const;

  static const size_t BNR1_SIZE = sizeof(GCBanner) - sizeof(GCBannerInformation) * 5;
  static const size_t BNR2_SIZE = sizeof(GCBanner);

  mutable std::map<Language, std::string> m_short_names;

  mutable std::map<Language, std::string> m_long_names;
  mutable std::map<Language, std::string> m_short_makers;
  mutable std::map<Language, std::string> m_long_makers;
  mutable std::map<Language, std::string> m_descriptions;

  mutable bool m_banner_loaded = false;
  mutable std::vector<u32> m_image_buffer;
  mutable int m_image_height = 0;
  mutable int m_image_width = 0;

  std::unique_ptr<IBlobReader> m_pReader;
};

}  // namespace
