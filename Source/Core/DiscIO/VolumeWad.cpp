// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <locale>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWad.h"
#include "DiscIO/WiiSaveBanner.h"

namespace DiscIO
{
VolumeWAD::VolumeWAD(std::unique_ptr<BlobReader> reader) : m_reader(std::move(reader))
{
  _assert_(m_reader);

  // Source: http://wiibrew.org/wiki/WAD_files
  m_hdr_size = m_reader->ReadSwapped<u32>(0x00).value_or(0);
  m_cert_size = m_reader->ReadSwapped<u32>(0x08).value_or(0);
  m_tick_size = m_reader->ReadSwapped<u32>(0x10).value_or(0);
  m_tmd_size = m_reader->ReadSwapped<u32>(0x14).value_or(0);
  m_data_size = m_reader->ReadSwapped<u32>(0x18).value_or(0);

  m_offset = Common::AlignUp(m_hdr_size, 0x40) + Common::AlignUp(m_cert_size, 0x40);
  m_tmd_offset = Common::AlignUp(m_hdr_size, 0x40) + Common::AlignUp(m_cert_size, 0x40) +
                 Common::AlignUp(m_tick_size, 0x40);
  m_opening_bnr_offset =
      m_tmd_offset + Common::AlignUp(m_tmd_size, 0x40) + Common::AlignUp(m_data_size, 0x40);

  if (!IOS::ES::IsValidTMDSize(m_tmd_size))
  {
    ERROR_LOG(DISCIO, "TMD is too large: %u bytes", m_tmd_size);
    return;
  }

  std::vector<u8> tmd_buffer(m_tmd_size);
  Read(m_tmd_offset, m_tmd_size, tmd_buffer.data());
  m_tmd.SetBytes(std::move(tmd_buffer));
}

VolumeWAD::~VolumeWAD()
{
}

bool VolumeWAD::Read(u64 offset, u64 length, u8* buffer, const Partition& partition) const
{
  if (partition != PARTITION_NONE)
    return false;

  return m_reader->Read(offset, length, buffer);
}

const FileSystem* VolumeWAD::GetFileSystem(const Partition& partition) const
{
  // TODO: Implement this?
  return nullptr;
}

Region VolumeWAD::GetRegion() const
{
  if (!m_tmd.IsValid())
    return Region::UNKNOWN_REGION;
  return m_tmd.GetRegion();
}

Country VolumeWAD::GetCountry(const Partition& partition) const
{
  if (!m_tmd.IsValid())
    return Country::COUNTRY_UNKNOWN;

  u8 country_code = static_cast<u8>(m_tmd.GetTitleId() & 0xff);
  if (country_code == 2)  // SYSMENU
    return TypicalCountryForRegion(GetSysMenuRegion(m_tmd.GetTitleVersion()));

  return CountrySwitch(country_code);
}

const IOS::ES::TMDReader& VolumeWAD::GetTMD(const Partition& partition) const
{
  return m_tmd;
}

std::string VolumeWAD::GetGameID(const Partition& partition) const
{
  return m_tmd.GetGameID();
}

std::string VolumeWAD::GetMakerID(const Partition& partition) const
{
  char temp[2];
  if (!Read(0x198 + m_tmd_offset, 2, (u8*)temp, partition))
    return "00";

  // Some weird channels use 0x0000 in place of the MakerID, so we need a check here
  const std::locale& c_locale = std::locale::classic();
  if (!std::isprint(temp[0], c_locale) || !std::isprint(temp[1], c_locale))
    return "00";

  return DecodeString(temp);
}

std::optional<u64> VolumeWAD::GetTitleID(const Partition& partition) const
{
  return ReadSwapped<u64>(m_offset + 0x01DC, partition);
}

std::optional<u16> VolumeWAD::GetRevision(const Partition& partition) const
{
  if (!m_tmd.IsValid())
    return {};

  return m_tmd.GetTitleVersion();
}

Platform VolumeWAD::GetVolumeType() const
{
  return Platform::WII_WAD;
}

std::map<Language, std::string> VolumeWAD::GetLongNames() const
{
  if (!m_tmd.IsValid() || !IOS::ES::IsChannel(m_tmd.GetTitleId()))
    return {};

  std::vector<char16_t> names(NAMES_TOTAL_CHARS);
  if (!Read(m_opening_bnr_offset + 0x9C, NAMES_TOTAL_BYTES, reinterpret_cast<u8*>(names.data())))
    return std::map<Language, std::string>();
  return ReadWiiNames(names);
}

std::vector<u32> VolumeWAD::GetBanner(int* width, int* height) const
{
  *width = 0;
  *height = 0;

  const std::optional<u64> title_id = GetTitleID();
  if (!title_id)
    return std::vector<u32>();

  return WiiSaveBanner(*title_id).GetBanner(width, height);
}

BlobType VolumeWAD::GetBlobType() const
{
  return m_reader->GetBlobType();
}

u64 VolumeWAD::GetSize() const
{
  return m_reader->GetDataSize();
}

u64 VolumeWAD::GetRawSize() const
{
  return m_reader->GetRawSize();
}

}  // namespace
