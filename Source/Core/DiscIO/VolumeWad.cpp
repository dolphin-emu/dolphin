// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWad.h"

namespace DiscIO
{
CVolumeWAD::CVolumeWAD(std::unique_ptr<IBlobReader> reader) : m_reader(std::move(reader))
{
  // Source: http://wiibrew.org/wiki/WAD_files
  ReadSwapped(0x00, &m_hdr_size, false);
  ReadSwapped(0x08, &m_cert_size, false);
  ReadSwapped(0x10, &m_tick_size, false);
  ReadSwapped(0x14, &m_tmd_size, false);
  ReadSwapped(0x18, &m_data_size, false);

  m_offset = Common::AlignUp(m_hdr_size, 0x40) + Common::AlignUp(m_cert_size, 0x40);
  m_tmd_offset = Common::AlignUp(m_hdr_size, 0x40) + Common::AlignUp(m_cert_size, 0x40) +
                 Common::AlignUp(m_tick_size, 0x40);
  m_opening_bnr_offset =
      m_tmd_offset + Common::AlignUp(m_tmd_size, 0x40) + Common::AlignUp(m_data_size, 0x40);
}

CVolumeWAD::~CVolumeWAD()
{
}

bool CVolumeWAD::Read(u64 offset, u64 length, u8* buffer, bool decrypt) const
{
  if (decrypt)
    PanicAlertT("Tried to decrypt data from a non-Wii volume");

  if (m_reader == nullptr)
    return false;

  return m_reader->Read(offset, length, buffer);
}

Region CVolumeWAD::GetRegion() const
{
  u8 country_code;
  if (!Read(m_tmd_offset + 0x0193, 1, &country_code))
    return Region::UNKNOWN_REGION;

  return RegionSwitchWii(country_code);
}

Country CVolumeWAD::GetCountry() const
{
  // read the last digit of the titleID in the ticket
  u8 country_code;
  if (!Read(m_tmd_offset + 0x0193, 1, &country_code))
    return Country::COUNTRY_UNKNOWN;

  if (country_code == 2)  // SYSMENU
  {
    u16 title_version = 0;
    Read(m_tmd_offset + 0x01dc, 2, (u8*)&title_version);
    country_code = GetSysMenuRegion(Common::swap16(title_version));
  }

  return CountrySwitch(country_code);
}

std::vector<u8> CVolumeWAD::GetTMD() const
{
  if (m_tmd_size > 1024 * 1024 * 4)
  {
    ERROR_LOG(DISCIO, "TMD is too large: %u bytes", m_tmd_size);
    return {};
  }
  std::vector<u8> buffer(m_tmd_size);
  Read(m_tmd_offset, m_tmd_size, buffer.data(), false);
  return buffer;
}

std::string CVolumeWAD::GetGameID() const
{
  char GameCode[6];
  if (!Read(m_offset + 0x01E0, 4, (u8*)GameCode))
    return "0";

  std::string temp = GetMakerID();
  GameCode[4] = temp.at(0);
  GameCode[5] = temp.at(1);

  return DecodeString(GameCode);
}

std::string CVolumeWAD::GetMakerID() const
{
  char temp[2] = {1};
  // Some weird channels use 0x0000 in place of the MakerID, so we need a check there
  if (!Read(0x198 + m_tmd_offset, 2, (u8*)temp) || temp[0] == 0 || temp[1] == 0)
    return "00";

  return DecodeString(temp);
}

bool CVolumeWAD::GetTitleID(u64* buffer) const
{
  if (!Read(m_offset + 0x01DC, sizeof(u64), reinterpret_cast<u8*>(buffer)))
    return false;

  *buffer = Common::swap64(*buffer);
  return true;
}

u16 CVolumeWAD::GetRevision() const
{
  u16 revision;
  if (!m_reader->Read(m_tmd_offset + 0x1dc, 2, (u8*)&revision))
    return 0;

  return Common::swap16(revision);
}

Platform CVolumeWAD::GetVolumeType() const
{
  return Platform::WII_WAD;
}

std::map<Language, std::string> CVolumeWAD::GetLongNames() const
{
  std::vector<u8> name_data(NAMES_TOTAL_BYTES);
  if (!Read(m_opening_bnr_offset + 0x9C, NAMES_TOTAL_BYTES, name_data.data()))
    return std::map<Language, std::string>();
  return ReadWiiNames(name_data);
}

std::vector<u32> CVolumeWAD::GetBanner(int* width, int* height) const
{
  *width = 0;
  *height = 0;

  u64 title_id;
  if (!GetTitleID(&title_id))
    return std::vector<u32>();

  return GetWiiBanner(width, height, title_id);
}

BlobType CVolumeWAD::GetBlobType() const
{
  return m_reader ? m_reader->GetBlobType() : BlobType::PLAIN;
}

u64 CVolumeWAD::GetSize() const
{
  return m_reader ? m_reader->GetDataSize() : 0;
}

u64 CVolumeWAD::GetRawSize() const
{
  return m_reader ? m_reader->GetRawSize() : 0;
}

}  // namespace
