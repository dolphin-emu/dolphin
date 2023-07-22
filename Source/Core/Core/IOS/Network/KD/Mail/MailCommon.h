// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/IOS/Network/KD/NWC24Config.h"

namespace IOS::HLE::NWC24::Mail
{
constexpr u32 MAIL_LIST_MAGIC = 0x57635466;  // WcTf

inline u32 PackData(u32 one, u32 two)
{
  return (one & 0xFFFFF) | two << 20;
}

enum class FlagOP
{
  Or,
  And
};

#pragma pack(push, 1)
struct MailListHeader final
{
  u32 magic;    // 'WcTf' 0x57635466
  u32 version;  // 4 in Wii Menu 4.x
  u32 number_of_mail;
  u32 total_entries;
  u32 total_size_of_messages;
  u32 filesize;
  u32 next_entry_id;
  u32 next_entry_offset;
  u32 unk2;
  u32 vff_free_space;
  u8 unk3[48];
  char mail_flag[40];
};

struct MultipartEntry final
{
  u32 offset;
  u32 size;
};

struct MailListEntry final
{
  u32 id;
  u32 flag;
  u32 msg_size;
  u32 app_id;
  u32 header_length;
  u32 tag;
  u32 wii_cmd;
  // Never written to for some reason
  u32 crc32;
  u64 from_friend_code;
  u32 minutes_since_1900;
  u32 padding;
  u8 always_1;
  u8 number_of_multipart_entries;
  u16 app_group;
  u32 packed_from;
  u32 packed_to;
  u32 packed_subject;
  u32 packed_charset;
  u32 packed_transfer_encoding;
  u32 message_offset;
  // Set to message_length if content transfer encoding is not base64.
  u32 encoded_length;
  MultipartEntry multipart_entries[2];
  u32 multipart_sizes[2];
  u32 multipart_content_types[2];
  u32 message_length;
  u32 dwc_id;
  u32 always_0x80000000;
  u32 padding3;
};

#pragma pack(pop)
}  // namespace IOS::HLE::NWC24::Mail
