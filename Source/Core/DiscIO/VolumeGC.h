// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Lazy.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
class BlobReader;
enum class BlobType;
enum class Country;
class FileSystem;
enum class Language;
enum class Region;
enum class Platform;

class VolumeGC : public Volume
{
public:
  VolumeGC(std::unique_ptr<BlobReader> reader);
  ~VolumeGC();
  bool Read(u64 _Offset, u64 _Length, u8* _pBuffer,
            const Partition& partition = PARTITION_NONE) const override;
  const FileSystem* GetFileSystem(const Partition& partition = PARTITION_NONE) const override;
  std::string GetGameID(const Partition& partition = PARTITION_NONE) const override;
  std::string GetMakerID(const Partition& partition = PARTITION_NONE) const override;
  std::optional<u16> GetRevision(const Partition& partition = PARTITION_NONE) const override;
  std::string GetInternalName(const Partition& partition = PARTITION_NONE) const override;
  std::map<Language, std::string> GetShortNames() const override;
  std::map<Language, std::string> GetLongNames() const override;
  std::map<Language, std::string> GetShortMakers() const override;
  std::map<Language, std::string> GetLongMakers() const override;
  std::map<Language, std::string> GetDescriptions() const override;
  std::vector<u32> GetBanner(int* width, int* height) const override;
  std::string GetApploaderDate(const Partition& partition = PARTITION_NONE) const override;
  std::optional<u8> GetDiscNumber(const Partition& partition = PARTITION_NONE) const override;

  Platform GetVolumeType() const override;
  Region GetRegion() const override;
  Country GetCountry(const Partition& partition = PARTITION_NONE) const override;
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

  struct ConvertedGCBanner
  {
    ConvertedGCBanner();
    ~ConvertedGCBanner();

    std::map<Language, std::string> short_names;
    std::map<Language, std::string> long_names;
    std::map<Language, std::string> short_makers;
    std::map<Language, std::string> long_makers;
    std::map<Language, std::string> descriptions;

    std::vector<u32> image_buffer;
    int image_height = 0;
    int image_width = 0;
  };

  ConvertedGCBanner LoadBannerFile() const;
  ConvertedGCBanner ExtractBannerInformation(const GCBanner& banner_file, bool is_bnr1) const;

  static const size_t BNR1_SIZE = sizeof(GCBanner) - sizeof(GCBannerInformation) * 5;
  static const size_t BNR2_SIZE = sizeof(GCBanner);

  Common::Lazy<ConvertedGCBanner> m_converted_banner;

  Common::Lazy<std::unique_ptr<FileSystem>> m_file_system;

  std::unique_ptr<BlobReader> m_pReader;
};

}  // namespace
