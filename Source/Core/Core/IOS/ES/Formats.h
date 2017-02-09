// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Utilities to manipulate files and formats from the Wii's ES module: tickets,
// TMD, and other title informations.

#pragma once

#include <array>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

namespace ES
{
std::vector<u8> AESDecode(const u8* key, u8* iv, const u8* src, u32 size);

class TMDReader final
{
public:
  TMDReader() = default;
  explicit TMDReader(const std::vector<u8>& bytes);
  explicit TMDReader(std::vector<u8>&& bytes);

  void SetBytes(const std::vector<u8>& bytes);
  void SetBytes(std::vector<u8>&& bytes);

  bool IsValid() const;

  u64 GetIOSId() const;
  u64 GetTitleId() const;

  struct Content
  {
    u32 id;
    u16 index;
    u16 type;
    u64 size;
    std::array<u8, 20> sha1;
  };
  u16 GetNumContents() const;
  bool GetContent(u16 index, Content* content) const;
  bool FindContentById(u32 id, Content* content) const;

  void DoState(PointerWrap& p);

private:
  std::vector<u8> m_bytes;
};

#pragma pack(push, 4)
struct TimeLimit
{
  u32 enabled;
  u32 seconds;
};

struct TicketView
{
  u32 view;
  u64 ticket_id;
  u32 device_id;
  u64 title_id;
  u16 access_mask;
  u32 permitted_title_id;
  u32 permitted_title_mask;
  u8 title_export_allowed;
  u8 common_key_index;
  u8 unknown2[0x30];
  u8 content_access_permissions[0x40];
  TimeLimit time_limits[8];
};
static_assert(sizeof(TicketView) == 0xd8, "TicketView has the wrong size");

struct Ticket
{
  u8 signature_issuer[0x40];
  u8 ecdh_key[0x3c];
  u8 unknown[0x03];
  u8 title_key[0x10];
  u64 ticket_id;
  u32 device_id;
  u64 title_id;
  u16 access_mask;
  u16 ticket_version;
  u32 permitted_title_id;
  u32 permitted_title_mask;
  u8 title_export_allowed;
  u8 common_key_index;
  u8 unknown2[0x30];
  u8 content_access_permissions[0x40];
  TimeLimit time_limits[8];
};
static_assert(sizeof(Ticket) == 356, "Ticket has the wrong size");
#pragma pack(pop)

class TicketReader final
{
public:
  TicketReader() = default;
  explicit TicketReader(const std::vector<u8>& bytes);
  explicit TicketReader(std::vector<u8>&& bytes);

  void SetBytes(const std::vector<u8>& bytes);
  void SetBytes(std::vector<u8>&& bytes);

  bool IsValid() const;

  const std::vector<u8>& GetRawTicket() const;
  u32 GetNumberOfTickets() const;
  u32 GetOffset() const;

  // Returns a "raw" ticket view, without byte swapping. Intended for use from ES.
  // Theoretically, a ticket file can contain one or more tickets. In practice, most (all?)
  // official titles only have one ticket, but IOS *does* have code to handle ticket files with
  // more than just one ticket and generate ticket views for them, so we implement it too.
  std::vector<u8> GetRawTicketView(u32 ticket_num) const;

  u64 GetTitleId() const;
  std::vector<u8> GetTitleKey() const;

private:
  std::vector<u8> m_bytes;
};
}  // namespace ES
