// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/ColorUtil.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "DiscIO/Blob.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Enums.h"
#include "DiscIO/FileSystemGCWii.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeGC.h"

#if defined(_MSC_VER) && _MSC_VER <= 1800
#define constexpr const
#endif

namespace DiscIO
{
VolumeGC::VolumeGC(std::unique_ptr<BlobReader> reader) : m_pReader(std::move(reader))
{
  _assert_(m_pReader);

  m_file_system = [this]() -> std::unique_ptr<FileSystem> {
    auto file_system = std::make_unique<FileSystemGCWii>(this, PARTITION_NONE);
    return file_system->IsValid() ? std::move(file_system) : nullptr;
  };

  m_converted_banner = [this] { return LoadBannerFile(); };
}

VolumeGC::~VolumeGC()
{
}

bool VolumeGC::Read(u64 _Offset, u64 _Length, u8* _pBuffer, const Partition& partition) const
{
  if (partition != PARTITION_NONE)
    return false;

  return m_pReader->Read(_Offset, _Length, _pBuffer);
}

const FileSystem* VolumeGC::GetFileSystem(const Partition& partition) const
{
  return m_file_system->get();
}

std::string VolumeGC::GetGameID(const Partition& partition) const
{
  static const std::string NO_UID("NO_UID");

  char ID[6];

  if (!Read(0, sizeof(ID), reinterpret_cast<u8*>(ID), partition))
  {
    PanicAlertT("Failed to read unique ID from disc image");
    return NO_UID;
  }

  return DecodeString(ID);
}

Region VolumeGC::GetRegion() const
{
  const std::optional<u32> region_code = ReadSwapped<u32>(0x458, PARTITION_NONE);
  if (!region_code)
    return Region::UNKNOWN_REGION;
  const Region region = static_cast<Region>(*region_code);
  return region <= Region::PAL ? region : Region::UNKNOWN_REGION;
}

Country VolumeGC::GetCountry(const Partition& partition) const
{
  // The 0 that we use as a default value is mapped to COUNTRY_UNKNOWN and UNKNOWN_REGION
  const u8 country = ReadSwapped<u8>(3, partition).value_or(0);
  const Region region = GetRegion();

  // Korean GC releases use NTSC-J.
  // E is normally used for America, but it's also used for English-language Korean GC releases.
  // K is used by games that are in the Korean language.
  // W means Taiwan for Wii games, but on the GC, it's used for English-language Korean releases.
  // (There doesn't seem to be any pattern to which of E and W is used for Korean GC releases.)
  if (region == Region::NTSC_J && (country == 'E' || country == 'K' || country == 'W'))
    return Country::COUNTRY_KOREA;

  if (RegionSwitchGC(country) != region)
    return TypicalCountryForRegion(region);

  return CountrySwitch(country);
}

std::string VolumeGC::GetMakerID(const Partition& partition) const
{
  char makerID[2];
  if (!Read(0x4, 0x2, (u8*)&makerID, partition))
    return std::string();

  return DecodeString(makerID);
}

std::optional<u16> VolumeGC::GetRevision(const Partition& partition) const
{
  std::optional<u8> revision = ReadSwapped<u8>(7, partition);
  return revision ? *revision : std::optional<u16>();
}

std::string VolumeGC::GetInternalName(const Partition& partition) const
{
  char name[0x60];
  if (Read(0x20, 0x60, (u8*)name, partition))
    return DecodeString(name);

  return "";
}

std::map<Language, std::string> VolumeGC::GetShortNames() const
{
  return m_converted_banner->short_names;
}

std::map<Language, std::string> VolumeGC::GetLongNames() const
{
  return m_converted_banner->long_names;
}

std::map<Language, std::string> VolumeGC::GetShortMakers() const
{
  return m_converted_banner->short_makers;
}

std::map<Language, std::string> VolumeGC::GetLongMakers() const
{
  return m_converted_banner->long_makers;
}

std::map<Language, std::string> VolumeGC::GetDescriptions() const
{
  return m_converted_banner->descriptions;
}

std::vector<u32> VolumeGC::GetBanner(int* width, int* height) const
{
  *width = m_converted_banner->image_width;
  *height = m_converted_banner->image_height;
  return m_converted_banner->image_buffer;
}

std::string VolumeGC::GetApploaderDate(const Partition& partition) const
{
  char date[16];
  if (!Read(0x2440, 0x10, (u8*)&date, partition))
    return std::string();

  return DecodeString(date);
}

BlobType VolumeGC::GetBlobType() const
{
  return m_pReader->GetBlobType();
}

u64 VolumeGC::GetSize() const
{
  return m_pReader->GetDataSize();
}

u64 VolumeGC::GetRawSize() const
{
  return m_pReader->GetRawSize();
}

std::optional<u8> VolumeGC::GetDiscNumber(const Partition& partition) const
{
  return ReadSwapped<u8>(6, partition);
}

Platform VolumeGC::GetVolumeType() const
{
  return Platform::GAMECUBE_DISC;
}

VolumeGC::ConvertedGCBanner VolumeGC::LoadBannerFile() const
{
  GCBanner banner_file;
  const u64 file_size = ReadFile(*this, PARTITION_NONE, "opening.bnr",
                                 reinterpret_cast<u8*>(&banner_file), sizeof(GCBanner));
  if (file_size < 4)
  {
    WARN_LOG(DISCIO, "Could not read opening.bnr.");
    return {};  // Return early so that we don't access the uninitialized banner_file.id
  }

  constexpr u32 BNR1_MAGIC = 0x31524e42;
  constexpr u32 BNR2_MAGIC = 0x32524e42;
  bool is_bnr1;
  if (banner_file.id == BNR1_MAGIC && file_size == BNR1_SIZE)
  {
    is_bnr1 = true;
  }
  else if (banner_file.id == BNR2_MAGIC && file_size == BNR2_SIZE)
  {
    is_bnr1 = false;
  }
  else
  {
    WARN_LOG(DISCIO, "Invalid opening.bnr. Type: %0x Size: %0zx", banner_file.id, file_size);
    return {};
  }

  return ExtractBannerInformation(banner_file, is_bnr1);
}

VolumeGC::ConvertedGCBanner VolumeGC::ExtractBannerInformation(const GCBanner& banner_file,
                                                               bool is_bnr1) const
{
  ConvertedGCBanner banner;

  u32 number_of_languages = 0;
  Language start_language = Language::LANGUAGE_UNKNOWN;

  if (is_bnr1)  // NTSC
  {
    bool is_japanese = GetRegion() == Region::NTSC_J;
    number_of_languages = 1;
    start_language = is_japanese ? Language::LANGUAGE_JAPANESE : Language::LANGUAGE_ENGLISH;
  }
  else  // PAL
  {
    number_of_languages = 6;
    start_language = Language::LANGUAGE_ENGLISH;
  }

  banner.image_width = GC_BANNER_WIDTH;
  banner.image_height = GC_BANNER_HEIGHT;
  banner.image_buffer = std::vector<u32>(GC_BANNER_WIDTH * GC_BANNER_HEIGHT);
  ColorUtil::decode5A3image(banner.image_buffer.data(), banner_file.image, GC_BANNER_WIDTH,
                            GC_BANNER_HEIGHT);

  for (u32 i = 0; i < number_of_languages; ++i)
  {
    const GCBannerInformation& info = banner_file.information[i];
    Language language = static_cast<Language>(static_cast<int>(start_language) + i);

    std::string description = DecodeString(info.description);
    if (!description.empty())
      banner.descriptions.emplace(language, description);

    std::string short_name = DecodeString(info.short_name);
    if (!short_name.empty())
      banner.short_names.emplace(language, short_name);

    std::string long_name = DecodeString(info.long_name);
    if (!long_name.empty())
      banner.long_names.emplace(language, long_name);

    std::string short_maker = DecodeString(info.short_maker);
    if (!short_maker.empty())
      banner.short_makers.emplace(language, short_maker);

    std::string long_maker = DecodeString(info.long_maker);
    if (!long_maker.empty())
      banner.long_makers.emplace(language, long_maker);
  }

  return banner;
}

VolumeGC::ConvertedGCBanner::ConvertedGCBanner() = default;
VolumeGC::ConvertedGCBanner::~ConvertedGCBanner() = default;
}  // namespace

#if defined(_MSC_VER) && _MSC_VER <= 1800
#undef constexpr
#endif
