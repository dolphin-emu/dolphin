// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <map>
#include <memory>
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
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeGC.h"

namespace DiscIO
{
CVolumeGC::CVolumeGC(std::unique_ptr<IBlobReader> reader) : m_pReader(std::move(reader))
{
  _assert_(m_pReader);
}

CVolumeGC::~CVolumeGC()
{
}

bool CVolumeGC::Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt) const
{
  if (decrypt)
    PanicAlertT("Tried to decrypt data from a non-Wii volume");

  return m_pReader->Read(_Offset, _Length, _pBuffer);
}

std::string CVolumeGC::GetGameID() const
{
  static const std::string NO_UID("NO_UID");

  char ID[6];

  if (!Read(0, sizeof(ID), reinterpret_cast<u8*>(ID)))
  {
    PanicAlertT("Failed to read unique ID from disc image");
    return NO_UID;
  }

  return DecodeString(ID);
}

Region CVolumeGC::GetRegion() const
{
  u8 country_code;
  if (!ReadSwapped(3, &country_code, false))
    return Region::UNKNOWN_REGION;

  return RegionSwitchGC(country_code);
}

Country CVolumeGC::GetCountry() const
{
  u8 country_code;
  if (!ReadSwapped(3, &country_code, false))
    return Country::COUNTRY_UNKNOWN;

  return CountrySwitch(country_code);
}

std::string CVolumeGC::GetMakerID() const
{
  char makerID[2];
  if (!Read(0x4, 0x2, (u8*)&makerID))
    return std::string();

  return DecodeString(makerID);
}

u16 CVolumeGC::GetRevision() const
{
  u8 revision;
  if (!ReadSwapped(7, &revision, false))
    return 0;

  return revision;
}

std::string CVolumeGC::GetInternalName() const
{
  char name[0x60];
  if (Read(0x20, 0x60, (u8*)name))
    return DecodeString(name);

  return "";
}

std::map<Language, std::string> CVolumeGC::GetShortNames() const
{
  LoadBannerFile();
  return m_short_names;
}

std::map<Language, std::string> CVolumeGC::GetLongNames() const
{
  LoadBannerFile();
  return m_long_names;
}

std::map<Language, std::string> CVolumeGC::GetShortMakers() const
{
  LoadBannerFile();
  return m_short_makers;
}

std::map<Language, std::string> CVolumeGC::GetLongMakers() const
{
  LoadBannerFile();
  return m_long_makers;
}

std::map<Language, std::string> CVolumeGC::GetDescriptions() const
{
  LoadBannerFile();
  return m_descriptions;
}

std::vector<u32> CVolumeGC::GetBanner(int* width, int* height) const
{
  LoadBannerFile();
  *width = m_image_width;
  *height = m_image_height;
  return m_image_buffer;
}

u64 CVolumeGC::GetFSTSize() const
{
  u32 size;
  if (!Read(0x428, 0x4, (u8*)&size))
    return 0;

  return Common::swap32(size);
}

std::string CVolumeGC::GetApploaderDate() const
{
  char date[16];
  if (!Read(0x2440, 0x10, (u8*)&date))
    return std::string();

  return DecodeString(date);
}

BlobType CVolumeGC::GetBlobType() const
{
  return m_pReader->GetBlobType();
}

u64 CVolumeGC::GetSize() const
{
  return m_pReader->GetDataSize();
}

u64 CVolumeGC::GetRawSize() const
{
  return m_pReader->GetRawSize();
}

u8 CVolumeGC::GetDiscNumber() const
{
  u8 disc_number;
  ReadSwapped(6, &disc_number, false);
  return disc_number;
}

Platform CVolumeGC::GetVolumeType() const
{
  return Platform::GAMECUBE_DISC;
}

void CVolumeGC::LoadBannerFile() const
{
  // If opening.bnr has been loaded already, return immediately
  if (m_banner_loaded)
    return;

  m_banner_loaded = true;

  GCBanner banner_file;
  std::unique_ptr<IFileSystem> file_system(CreateFileSystem(this));
  size_t file_size = (size_t)file_system->GetFileSize("opening.bnr");

  constexpr int BNR1_MAGIC = 0x31524e42;
  constexpr int BNR2_MAGIC = 0x32524e42;
  if (file_size != BNR1_SIZE && file_size != BNR2_SIZE)
  {
    WARN_LOG(DISCIO, "Invalid opening.bnr. Size: %0zx", file_size);
    return;
  }

  file_system->ReadFile("opening.bnr", reinterpret_cast<u8*>(&banner_file), file_size);

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
    return;
  }

  ExtractBannerInformation(banner_file, is_bnr1);
}

void CVolumeGC::ExtractBannerInformation(const GCBanner& banner_file, bool is_bnr1) const
{
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

  m_image_width = GC_BANNER_WIDTH;
  m_image_height = GC_BANNER_HEIGHT;
  m_image_buffer = std::vector<u32>(m_image_width * m_image_height);
  ColorUtil::decode5A3image(m_image_buffer.data(), banner_file.image, m_image_width,
                            m_image_height);

  for (u32 i = 0; i < number_of_languages; ++i)
  {
    const GCBannerInformation& info = banner_file.information[i];
    Language language = static_cast<Language>(static_cast<int>(start_language) + i);

    std::string description = DecodeString(info.description);
    if (!description.empty())
      m_descriptions[language] = description;

    std::string short_name = DecodeString(info.short_name);
    if (!short_name.empty())
      m_short_names[language] = short_name;

    std::string long_name = DecodeString(info.long_name);
    if (!long_name.empty())
      m_long_names[language] = long_name;

    std::string short_maker = DecodeString(info.short_maker);
    if (!short_maker.empty())
      m_short_makers[language] = short_maker;

    std::string long_maker = DecodeString(info.long_maker);
    if (!long_maker.empty())
      m_long_makers[language] = long_maker;
  }
}

}  // namespace
