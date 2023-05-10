// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/IOS/Network/KD/NWC24Config.h"

namespace IOS::HLE
{
namespace FS
{
class FileSystem;
}
namespace NWC24
{
constexpr const char RECEIVE_BOX_PATH[] = "/" WII_WC24CONF_DIR "/mbox"
                                          "/wc24recv.mbx";
class WC24ReceiveList final
{
public:
  static std::string GetMailPath(u32 index);

  explicit WC24ReceiveList(std::shared_ptr<FS::FileSystem> fs);
  void ReadReceiveList();
  bool CheckReceiveList() const;
  void WriteReceiveList() const;

  u32 GetIndex(u32 index) const;
  u32 GetNextEntryId() const;
  u32 GetNextEntryIndex() const;

  void FinalizeEntry(u32 index);
  void ClearEntry(u32 index);
  void SetMessageId(u32 index, u32 id);
  void SetMessageSize(u32 index, u32 size);
  void SetHeaderLength(u32 index, u32 size);
  void SetMessageOffset(u32 index, u32 offset);
  void SetEncodedMessageLength(u32 index, u32 size);
  void SetMessageLength(u32 index, u32 size);
  void SetPackedContentTransferEncoding(u32 index, u32 offset, u32 size);
  void SetPackedCharset(u32 index, u32 offset, u32 size);
  void SetTime(u32 index, u32 time);
  void SetFromFriendCode(u32 index, u64 friend_code);
  void SetPackedFrom(u32 index, u32 offset, u32 size);
  void SetPackedSubject(u32 index, u32 offset, u32 size);
  void SetPackedTo(u32 index, u32 offset, u32 size);
  void SetWiiAppId(u32 index, u32 id);
  void SetWiiAppGroupId(u32 index, u32 id);
  void SetWiiCmd(u32 index, u32 cmd);

private:
  static constexpr u32 RECEIVE_LIST_MAGIC = 0x57635466;  // WcTf
  static constexpr u32 MAX_ENTRIES = 255;

  static u32 PackData(u32 one, u32 two);

#pragma pack(push, 1)
  struct ReceiveListHeader final
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

  struct ReceiveListEntry final
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
    u32 app_group;
    u32 packed_from;
    u32 packed_to;
    u32 packed_subject;
    u32 packed_charset;
    u32 packed_transfer_encoding;
    u32 message_offset;
    // Set to message_length if content transfer encoding is not base64.
    u32 encoded_length;
    u8 padding2[24];
    u64 unk;
    u32 message_length;
    u32 dwc_id;
    u32 always_0x80000000;
    u32 padding3;
  };

  struct ReceiveList final
  {
    ReceiveListHeader header;
    ReceiveListEntry entries[MAX_ENTRIES];
  };
#pragma pack(pop)

  ReceiveList m_data;
  std::shared_ptr<FS::FileSystem> m_fs;
};
}  // namespace NWC24
}  // namespace IOS::HLE
