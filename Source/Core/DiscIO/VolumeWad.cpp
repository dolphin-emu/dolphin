// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/VolumeWad.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/AES.h"
#include "Common/Crypto/SHA1.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/IOS/IOSC.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
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
  m_opening_bnr_size = m_reader->ReadSwapped<u32>(0x1C).value_or(0);

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
    ERROR_LOG_FMT(DISCIO, "TMD is too large: {} bytes", m_tmd_size);
    return;
  }

  std::vector<u8> tmd_buffer(m_tmd_size);
  Read(m_tmd_offset, m_tmd_size, tmd_buffer.data());
  m_tmd.SetBytes(std::move(tmd_buffer));

  m_cert_chain.resize(m_cert_chain_size);
  Read(m_cert_chain_offset, m_cert_chain_size, m_cert_chain.data());
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

std::vector<u8> VolumeWAD::GetContent(u16 index) const
{
  u64 offset = m_data_offset;
  for (const IOS::ES::Content& content : m_tmd.GetContents())
  {
    const u64 aligned_size = Common::AlignUp(content.size, 0x40);
    if (content.index == index)
    {
      std::vector<u8> data(aligned_size);
      if (!m_reader->Read(offset, aligned_size, data.data()))
        return {};
      return data;
    }
    offset += aligned_size;
  }
  return {};
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

bool VolumeWAD::CheckContentIntegrity(const IOS::ES::Content& content,
                                      const std::vector<u8>& encrypted_data,
                                      const IOS::ES::TicketReader& ticket) const
{
  if (encrypted_data.size() != Common::AlignUp(content.size, 0x40))
    return false;

  auto context = Common::AES::CreateContextDecrypt(ticket.GetTitleKey().data());

  std::array<u8, 16> iv{};
  iv[0] = static_cast<u8>(content.index >> 8);
  iv[1] = static_cast<u8>(content.index & 0xFF);

  std::vector<u8> decrypted_data(encrypted_data.size());
  context->Crypt(iv.data(), encrypted_data.data(), decrypted_data.data(), decrypted_data.size());

  return Common::SHA1::CalculateDigest(decrypted_data.data(), content.size) == content.sha1;
}

bool VolumeWAD::CheckContentIntegrity(const IOS::ES::Content& content, u64 content_offset,
                                      const IOS::ES::TicketReader& ticket) const
{
  std::vector<u8> encrypted_data(Common::AlignUp(content.size, 0x40));
  if (!m_reader->Read(content_offset, encrypted_data.size(), encrypted_data.data()))
    return false;
  return CheckContentIntegrity(content, encrypted_data, ticket);
}

IOS::ES::TicketReader VolumeWAD::GetTicketWithFixedCommonKey() const
{
  if (!m_ticket.IsValid() || !m_tmd.IsValid())
    return m_ticket;

  const std::vector<u8> sig = m_ticket.GetSignatureData();
  if (!std::all_of(sig.cbegin(), sig.cend(), [](u8 a) { return a == 0; }))
  {
    // This does not look like a typical "invalid common key index" ticket, so let's assume
    // the index is correct. This saves some time when reading properly signed titles.
    return m_ticket;
  }

  const std::vector<IOS::ES::Content> contents = m_tmd.GetContents();
  if (contents.empty())
    return m_ticket;

  // Find the smallest content so that we spend as little time as possible in CheckContentIntegrity
  IOS::ES::Content smallest_content = contents[0];
  u64 offset_of_smallest_content = m_data_offset;

  u64 offset = m_data_offset;
  for (const IOS::ES::Content& content : contents)
  {
    if (content.size < smallest_content.size)
    {
      smallest_content = content;
      offset_of_smallest_content = offset;
    }
    offset += Common::AlignUp(content.size, 0x40);
  }

  std::vector<u8> content_data(Common::AlignUp(smallest_content.size, 0x40));
  if (!m_reader->Read(offset_of_smallest_content, content_data.size(), content_data.data()))
    return m_ticket;

  const u8 specified_index = m_ticket.GetCommonKeyIndex();
  if (specified_index < IOS::HLE::IOSC::COMMON_KEY_HANDLES.size() &&
      CheckContentIntegrity(smallest_content, content_data, m_ticket))
  {
    return m_ticket;  // The common key index is already correct
  }

  // Try every common key index except the one we already tried
  IOS::ES::TicketReader new_ticket = m_ticket;
  for (u8 i = 0; i < IOS::HLE::IOSC::COMMON_KEY_HANDLES.size(); ++i)
  {
    if (i != specified_index)
    {
      new_ticket.OverwriteCommonKeyIndex(i);
      if (CheckContentIntegrity(smallest_content, content_data, new_ticket))
        return new_ticket;  // We've found the common key index that should be used
    }
  }

  ERROR_LOG_FMT(DISCIO, "Couldn't find valid common key for WAD file ({} specified)",
                specified_index);
  return m_ticket;
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
  if (!Common::IsPrintableCharacter(temp[0]) || !Common::IsPrintableCharacter(temp[1]))
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

bool VolumeWAD::IsDatelDisc() const
{
  return false;
}

bool VolumeWAD::IsNKit() const
{
  return false;
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

u64 VolumeWAD::GetDataSize() const
{
  return m_reader->GetDataSize();
}

DataSizeType VolumeWAD::GetDataSizeType() const
{
  return m_reader->GetDataSizeType();
}

u64 VolumeWAD::GetRawSize() const
{
  return m_reader->GetRawSize();
}

const BlobReader& VolumeWAD::GetBlobReader() const
{
  return *m_reader;
}

std::array<u8, 20> VolumeWAD::GetSyncHash() const
{
  // We can skip hashing the contents since the TMD contains hashes of the contents.
  // We specifically don't hash the ticket, since its console ID can differ without any problems.

  auto context = Common::SHA1::CreateContext();

  AddTMDToSyncHash(context.get(), PARTITION_NONE);

  ReadAndAddToSyncHash(context.get(), m_opening_bnr_offset, m_opening_bnr_size, PARTITION_NONE);

  return context->Finish();
}

}  // namespace DiscIO
