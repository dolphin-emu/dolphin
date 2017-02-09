// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/Formats.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

#include <mbedtls/aes.h>

#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"

namespace ES
{
std::vector<u8> AESDecode(const u8* key, u8* iv, const u8* src, u32 size)
{
  mbedtls_aes_context aes_ctx;
  std::vector<u8> buffer(size);

  mbedtls_aes_setkey_dec(&aes_ctx, key, 128);
  mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, size, iv, src, buffer.data());

  return buffer;
}

TMDReader::TMDReader(const std::vector<u8>& bytes) : m_bytes(bytes)
{
}

TMDReader::TMDReader(std::vector<u8>&& bytes) : m_bytes(std::move(bytes))
{
}

void TMDReader::SetBytes(const std::vector<u8>& bytes)
{
  m_bytes = bytes;
}

void TMDReader::SetBytes(std::vector<u8>&& bytes)
{
  m_bytes = std::move(bytes);
}

bool TMDReader::IsValid() const
{
  if (m_bytes.size() < 0x1E4)
  {
    // TMD is too small to contain its base fields.
    return false;
  }

  if (m_bytes.size() < 0x1E4 + GetNumContents() * 36u)
  {
    // TMD is too small to contain all its expected content entries.
    return false;
  }

  return true;
}

u64 TMDReader::GetIOSId() const
{
  return Common::swap64(m_bytes.data() + 0x184);
}

u64 TMDReader::GetTitleId() const
{
  return Common::swap64(m_bytes.data() + 0x18C);
}

u16 TMDReader::GetNumContents() const
{
  return Common::swap16(m_bytes.data() + 0x1DE);
}

bool TMDReader::GetContent(u16 index, Content* content) const
{
  if (index >= GetNumContents())
  {
    return false;
  }

  const u8* content_base = m_bytes.data() + 0x1E4 + index * 36;
  content->id = Common::swap32(content_base);
  content->index = Common::swap16(content_base + 4);
  content->type = Common::swap16(content_base + 6);
  content->size = Common::swap64(content_base + 8);
  std::copy(content_base + 16, content_base + 36, content->sha1.begin());

  return true;
}

bool TMDReader::FindContentById(u32 id, Content* content) const
{
  for (u16 index = 0; index < GetNumContents(); ++index)
  {
    if (!GetContent(index, content))
    {
      return false;
    }
    if (content->id == id)
    {
      return true;
    }
  }
  return false;
}

void TMDReader::DoState(PointerWrap& p)
{
  p.Do(m_bytes);
}

TicketReader::TicketReader(const std::vector<u8>& bytes) : m_bytes(bytes)
{
}

TicketReader::TicketReader(std::vector<u8>&& bytes) : m_bytes(std::move(bytes))
{
}

void TicketReader::SetBytes(const std::vector<u8>& bytes)
{
  m_bytes = bytes;
}

void TicketReader::SetBytes(std::vector<u8>&& bytes)
{
  m_bytes = std::move(bytes);
}

bool TicketReader::IsValid() const
{
  // Too small for the signature type.
  if (m_bytes.size() < sizeof(u32))
    return false;

  u32 ticket_offset = GetOffset();
  if (ticket_offset == 0)
    return false;

  // Too small for the ticket itself.
  if (m_bytes.size() < ticket_offset + sizeof(Ticket))
    return false;

  return true;
}

u32 TicketReader::GetNumberOfTickets() const
{
  return static_cast<u32>(m_bytes.size() / (GetOffset() + sizeof(Ticket)));
}

u32 TicketReader::GetOffset() const
{
  u32 signature_type = Common::swap32(m_bytes.data());
  if (signature_type == 0x10000)  // RSA4096
    return 576;
  if (signature_type == 0x10001)  // RSA2048
    return 320;
  if (signature_type == 0x10002)  // ECDSA
    return 128;

  ERROR_LOG(COMMON, "Invalid ticket signature type: %08x", signature_type);
  return 0;
}

const std::vector<u8>& TicketReader::GetRawTicket() const
{
  return m_bytes;
}

std::vector<u8> TicketReader::GetRawTicketView(u32 ticket_num) const
{
  // A ticket view is composed of a view ID + part of a ticket starting from the ticket_id field.
  std::vector<u8> view{sizeof(TicketView)};

  u32 view_id = Common::swap32(ticket_num);
  std::memcpy(view.data(), &view_id, sizeof(view_id));

  const size_t ticket_start = (GetOffset() + sizeof(Ticket)) * ticket_num;
  const size_t view_start = ticket_start + offsetof(Ticket, ticket_id);
  std::memcpy(view.data() + sizeof(view_id), &m_bytes[view_start], sizeof(view) - sizeof(view_id));

  return view;
}

u64 TicketReader::GetTitleId() const
{
  return Common::swap64(m_bytes.data() + GetOffset() + offsetof(Ticket, title_id));
}

std::vector<u8> TicketReader::GetTitleKey() const
{
  const u8 common_key[16] = {0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4,
                             0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7};
  u8 iv[16] = {};
  std::copy_n(&m_bytes[GetOffset() + offsetof(Ticket, title_id)], sizeof(Ticket::title_id), iv);
  return AESDecode(common_key, iv, &m_bytes[GetOffset() + offsetof(Ticket, title_key)], 16);
}
}  // namespace ES
