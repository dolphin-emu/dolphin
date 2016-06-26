// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Common/ColorUtil.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/FileMonitor.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeGC.h"

namespace DiscIO
{
CVolumeGC::CVolumeGC(std::unique_ptr<IBlobReader> reader) : m_pReader(std::move(reader))
{
}

CVolumeGC::~CVolumeGC() = default;

bool CVolumeGC::Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt) const
{
  if (decrypt)
    PanicAlertT("Tried to decrypt data from a non-Wii volume");

  if (m_pReader == nullptr)
    return false;

  FileMon::FindFilename(_Offset);

  return m_pReader->Read(_Offset, _Length, _pBuffer);
}

std::string CVolumeGC::GetUniqueID() const
{
  static const std::string NO_UID("NO_UID");
  if (m_pReader == nullptr)
    return NO_UID;

  char ID[6];

  if (!Read(0, sizeof(ID), reinterpret_cast<u8*>(ID)))
  {
    PanicAlertT("Failed to read unique ID from disc image");
    return NO_UID;
  }

  return DecodeString(ID);
}

IVolume::ECountry CVolumeGC::GetCountry() const
{
  if (!m_pReader)
    return COUNTRY_UNKNOWN;

  u8 country_code;
  m_pReader->Read(3, 1, &country_code);

  return CountrySwitch(country_code);
}

std::string CVolumeGC::GetMakerID() const
{
  if (m_pReader == nullptr)
    return std::string();

  char makerID[2];
  if (!Read(0x4, 0x2, (u8*)&makerID))
    return std::string();

  return DecodeString(makerID);
}

u16 CVolumeGC::GetRevision() const
{
  if (!m_pReader)
    return 0;

  u8 revision;
  if (!Read(7, 1, &revision))
    return 0;

  return revision;
}

std::string CVolumeGC::GetInternalName() const
{
  char name[0x60];
  if (m_pReader != nullptr && Read(0x20, 0x60, (u8*)name))
    return DecodeString(name);

  return "";
}

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetShortNames() const
{
  LoadBannerFile();
  return m_short_names;
}

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetLongNames() const
{
  LoadBannerFile();
  return m_long_names;
}

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetShortMakers() const
{
  LoadBannerFile();
  return m_short_makers;
}

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetLongMakers() const
{
  LoadBannerFile();
  return m_long_makers;
}

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetDescriptions() const
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
  if (m_pReader == nullptr)
    return 0;

  u32 size;
  if (!Read(0x428, 0x4, (u8*)&size))
    return 0;

  return Common::swap32(size);
}

std::string CVolumeGC::GetApploaderDate() const
{
  if (m_pReader == nullptr)
    return std::string();

  char date[16];
  if (!Read(0x2440, 0x10, (u8*)&date))
    return std::string();

  return DecodeString(date);
}

BlobType CVolumeGC::GetBlobType() const
{
  return m_pReader ? m_pReader->GetBlobType() : BlobType::PLAIN;
}

u64 CVolumeGC::GetSize() const
{
  if (m_pReader)
    return m_pReader->GetDataSize();
  else
    return 0;
}

u64 CVolumeGC::GetRawSize() const
{
  if (m_pReader)
    return m_pReader->GetRawSize();
  else
    return 0;
}

u8 CVolumeGC::GetDiscNumber() const
{
  u8 disc_number;
  Read(6, 1, &disc_number);
  return disc_number;
}

IVolume::EPlatform CVolumeGC::GetVolumeType() const
{
  return GAMECUBE_DISC;
}

void CVolumeGC::LoadBannerFile() const
{
  // If opening.bnr has been loaded already, return immediately
  if (m_banner_loaded)
    return;

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
  m_banner_loaded = true;
}

void CVolumeGC::ExtractBannerInformation(const GCBanner& banner_file, bool is_bnr1) const
{
  u32 number_of_languages = 0;
  ELanguage start_language = LANGUAGE_UNKNOWN;
  bool is_japanese = GetCountry() == ECountry::COUNTRY_JAPAN;

  if (is_bnr1)  // NTSC
  {
    number_of_languages = 1;
    start_language = is_japanese ? ELanguage::LANGUAGE_JAPANESE : ELanguage::LANGUAGE_ENGLISH;
  }
  else  // PAL
  {
    number_of_languages = 6;
    start_language = ELanguage::LANGUAGE_ENGLISH;
  }

  m_image_width = GC_BANNER_WIDTH;
  m_image_height = GC_BANNER_HEIGHT;
  m_image_buffer = std::vector<u32>(m_image_width * m_image_height);
  ColorUtil::decode5A3image(m_image_buffer.data(), banner_file.image, m_image_width,
                            m_image_height);

  for (u32 i = 0; i < number_of_languages; ++i)
  {
    const GCBannerInformation& info = banner_file.information[i];
    ELanguage language = static_cast<ELanguage>(start_language + i);

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
