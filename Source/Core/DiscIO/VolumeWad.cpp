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
  ASSERT(m_reader);

  // Source: http://wiibrew.org/wiki/WAD_files
  m_hdr_size = m_reader->ReadSwapped<u32>(0x00).value_or(0);
  m_cert_chain_size = m_reader->ReadSwapped<u32>(0x08).value_or(0);
  m_ticket_size = m_reader->ReadSwapped<u32>(0x10).value_or(0);
  m_tmd_size = m_reader->ReadSwapped<u32>(0x14).value_or(0);
  m_data_size = m_reader->ReadSwapped<u32>(0x18).value_or(0);

  m_cert_chain_offset = Common::AlignUp(m_hdr_size, 0x40);
  m_ticket_offset = m_cert_chain_offset + Common::AlignUp(m_cert_chain_size, 0x40);
  m_tmd_offset = m_ticket_offset + Common::AlignUp(m_ticket_size, 0x40);
  m_data_offset = m_tmd_offset + Common::AlignUp(m_tmd_size, 0x40);
  m_opening_bnr_offset = m_data_offset + Common::AlignUp(m_data_size, 0x40);

  std::vector<u8> ticket_buffer(m_ticket_size);
  Read(m_ticket_offset, m_ticket_size, ticket_buffer.data());
  m_ticket.SetBytes(std::move(ticket_buffer));

  if (!IOS::ES::IsValidTMDSize(m_tmd_size))
  {
    ERROR_LOG(DISCIO, "TMD is too large: %u bytes", m_tmd_size);
    return;
  }

  std::vector<u8> tmd_buffer(m_tmd_size);
  Read(m_tmd_offset, m_tmd_size, tmd_buffer.data());
  m_tmd.SetBytes(std::move(tmd_buffer));

  m_cert_chain.resize(m_cert_chain_size);
  Read(m_cert_chain_offset, m_cert_chain_size, m_cert_chain.data());
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
    return Region::Unknown;
  return m_tmd.GetRegion();
}

Country VolumeWAD::GetCountry(const Partition& partition) const
{
  if (!m_tmd.IsValid())
    return Country::Unknown;

  const u8 country_byte = static_cast<u8>(m_tmd.GetTitleId() & 0xff);
  if (country_byte == 2)  // SYSMENU
    return TypicalCountryForRegion(GetSysMenuRegion(m_tmd.GetTitleVersion()));

  const Region region = GetRegion();
  const std::optional<u16> revision = GetRevision();
  if (CountryCodeToRegion(country_byte, Platform::WiiWAD, region, revision) != region)
    return TypicalCountryForRegion(region);

  return CountryCodeToCountry(country_byte, Platform::WiiWAD, region, revision);
}

const IOS::ES::TicketReader& VolumeWAD::GetTicket(const Partition& partition) const
{
  return m_ticket;
}

const IOS::ES::TMDReader& VolumeWAD::GetTMD(const Partition& partition) const
{
  return m_tmd;
}

const std::vector<u8>& VolumeWAD::GetCertificateChain(const Partition& partition) const
{
  return m_cert_chain;
}

std::vector<u64> VolumeWAD::GetContentOffsets() const
{
  const std::vector<IOS::ES::Content> contents = m_tmd.GetContents();
  std::vector<u64> content_offsets;
  content_offsets.reserve(contents.size());
  u64 offset = m_data_offset;
  for (const IOS::ES::Content& content : contents)
  {
    content_offsets.emplace_back(offset);
    offset += Common::AlignUp(content.size, 0x40);
  }

  return content_offsets;
}

std::string VolumeWAD::GetGameID(const Partition& partition) const
{
  return m_tmd.GetGameID();
}

std::string VolumeWAD::GetGameTDBID(const Partition& partition) const
{
  return m_tmd.GetGameTDBID();
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
  return ReadSwapped<u64>(m_ticket_offset + 0x01DC, partition);
}

std::optional<u16> VolumeWAD::GetRevision(const Partition& partition) const
{
  if (!m_tmd.IsValid())
    return {};

  return m_tmd.GetTitleVersion();
}

Platform VolumeWAD::GetVolumeType() const
{
  return Platform::WiiWAD;
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

std::vector<u32> VolumeWAD::GetBanner(u32* width, u32* height) const
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

bool VolumeWAD::IsSizeAccurate() const
{
  return m_reader->IsDataSizeAccurate();
}

u64 VolumeWAD::GetRawSize() const
{
  return m_reader->GetRawSize();
}

}  // namespace DiscIO
