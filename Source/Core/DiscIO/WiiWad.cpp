// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <memory>
#include <string>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Blob.h"
#include "DiscIO/WiiWad.h"

namespace DiscIO
{
namespace
{
std::vector<u8> CreateWADEntry(IBlobReader& reader, u32 size, u64 offset)
{
  if (size == 0)
    return {};

  std::vector<u8> buffer(size);

  if (!reader.Read(offset, size, buffer.data()))
  {
    ERROR_LOG(DISCIO, "WiiWAD: Could not read from file");
    PanicAlertT("WiiWAD: Could not read from file");
  }

  return buffer;
}

bool IsWiiWAD(IBlobReader& reader)
{
  u32 header_size = 0;
  u32 header_type = 0;
  reader.ReadSwapped(0x0, &header_size);
  reader.ReadSwapped(0x4, &header_type);
  return header_size == 0x20 && (header_type == 0x49730000 || header_type == 0x69620000);
}
}  // Anonymous namespace

WiiWAD::WiiWAD(const std::string& name) : m_reader(CreateBlobReader(name))
{
  if (m_reader == nullptr || File::IsDirectory(name))
  {
    m_valid = false;
    return;
  }

  m_valid = ParseWAD();
}

WiiWAD::~WiiWAD()
{
}

bool WiiWAD::ParseWAD()
{
  if (!IsWiiWAD(*m_reader))
    return false;

  u32 certificate_chain_size;
  u32 reserved;
  u32 ticket_size;
  u32 tmd_size;
  u32 data_app_size;
  u32 footer_size;

  if (!m_reader->ReadSwapped(0x08, &certificate_chain_size) ||
      !m_reader->ReadSwapped(0x0C, &reserved) ||
      !m_reader->ReadSwapped(0x10, &ticket_size) ||
      !m_reader->ReadSwapped(0x14, &tmd_size) ||
      !m_reader->ReadSwapped(0x18, &data_app_size) ||
      !m_reader->ReadSwapped(0x1C, &footer_size))
    return false;

  if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG)
    _dbg_assert_msg_(BOOT, reserved == 0x00, "WiiWAD: Reserved must be 0x00");

  u32 offset = 0x40;
  m_certificate_chain = CreateWADEntry(*m_reader, certificate_chain_size, offset);
  offset += Common::AlignUp(certificate_chain_size, 0x40);
  m_ticket.SetBytes(CreateWADEntry(*m_reader, ticket_size, offset));
  offset += Common::AlignUp(ticket_size, 0x40);
  m_tmd.SetBytes(CreateWADEntry(*m_reader, tmd_size, offset));
  offset += Common::AlignUp(tmd_size, 0x40);
  m_data_app_offset = offset;
  m_data_app = CreateWADEntry(*m_reader, data_app_size, offset);
  offset += Common::AlignUp(data_app_size, 0x40);
  m_footer = CreateWADEntry(*m_reader, footer_size, offset);
  offset += Common::AlignUp(footer_size, 0x40);

  return true;
}

std::vector<u8> WiiWAD::GetContent(u16 index) const
{
  u64 offset = m_data_app_offset;
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
}  // namespace DiscIO
