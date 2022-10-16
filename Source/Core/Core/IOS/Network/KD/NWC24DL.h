// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include "Common/CommonTypes.h"
#include "Core/IOS/Network/KD/WC24File.h"

namespace IOS::HLE
{
namespace FS
{
class FileSystem;
}
namespace NWC24
{
class NWC24Dl final
{
public:
  explicit NWC24Dl(std::shared_ptr<FS::FileSystem> fs);

  void ReadDlList();
  void WriteDlList() const;

  s32 CheckNwc24DlList() const;

  bool DoesEntryExist(u16 entry_index);
  bool IsEncrypted(u16 entry_index) const;
  std::string GetVFFContentName(u16 entry_index, std::optional<u8> subtask_id) const;
  std::string GetDownloadURL(u16 entry_index, std::optional<u8> subtask_id) const;
  std::string GetVFFPath(u16 entry_index) const;
  WC24PubkMod GetWC24PubkMod(u16 entry_index) const;

  u32 Magic() const;
  void SetMagic(u32 magic);

  u32 Version() const;
  void SetVersion(u32 version);

  static constexpr u32 MAX_ENTRIES = 120;

private:
  static constexpr u32 DL_LIST_MAGIC = 0x5763446C;  // WcDl
  static constexpr u32 MAX_SUBENTRIES = 32;

  enum EntryType : u8
  {
    UNK = 1,
    MAIL,
    CHANNEL_CONTENT,
    UNUSED = 0xff
  };

#pragma pack(push, 1)
  // The following format is partially based off of https://wiibrew.org/wiki//dev/net/kd/request.
  struct DLListHeader final
  {
    u32 magic;    // 'WcDl' 0x5763446c
    u32 version;  // must be 1
    u32 unk1;
    u32 unk2;
    u16 max_subentries;  // Asserted to be less than max_entries
    u16 reserved_mailnum;
    u16 max_entries;
    u8 reserved[106];
  };

  struct DLListRecord final
  {
    u32 low_title_id;
    u32 next_dl_timestamp;
    u32 last_modified_timestamp;
    u8 flags;
    u8 padding[3];
  };

  struct DLListEntry final
  {
    u16 index;
    EntryType type;
    u8 record_flags;
    u32 flags;
    u32 high_title_id;
    u32 low_title_id;
    u32 unknown1;
    u16 group_id;
    u16 padding1;
    u16 remaining_downloads;
    u16 error_count;
    u16 dl_frequency;
    u16 dl_frequency_when_err;
    s32 error_code;
    u8 subtask_id;
    u8 subtask_type;
    u8 subtask_flags;
    u8 padding2;
    u32 subtask_bitmask;
    s32 unknown2;
    u32 dl_timestamp;  // Last DL time
    u32 subtask_timestamps[32];
    char dl_url[236];
    char filename[64];
    u8 unk6[29];
    u8 should_use_rootca;
    u16 unknown3;
  };

  struct DLList final
  {
    DLListHeader header;
    DLListRecord records[MAX_ENTRIES];
    DLListEntry entries[MAX_ENTRIES];
  };
#pragma pack(pop)

  std::shared_ptr<FS::FileSystem> m_fs;
  DLList m_data;
};
}  // namespace NWC24
}  // namespace IOS::HLE
