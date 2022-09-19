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
// TODO: -3 for error code

class NWC24Dl final
{
public:
  explicit NWC24Dl(std::shared_ptr<FS::FileSystem> fs);

  void ReadDlList();
  void WriteDlList() const;
  void ResetConfig();

  s32 CheckNwc24DlList() const;

  bool isEncrypted(u16 entry_index) const;
  std::string GetVFFContentName(u16 entry_index, std::optional<u8> subtask_id) const;
  std::string GetDownloadURL(u16 entry_index, std::optional<u8> subtask_id) const;
  std::string GetVFFPath(u16 entry_index) const;
  WC24PubkMod GetWC24PubkMod(u16 entry_index) const;
  u8 SubtaskID(u16 entry_index) const;

  u32 Magic() const;
  void SetMagic(u32 magic);

  u32 Version() const;
  void SetVersion(u32 version);

private:
  enum
  {
    DL_LIST_MAGIC = 'WcDl',  // 'WcDl'
    MAX_SUBENTRIES = 0x20,   // 32
    MAX_ENTRIES = 0x78,      // 120
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
    uint16_t index;
    uint8_t type; /* TODO: make enum */
    uint8_t record_flags;
    uint32_t flags;
    uint32_t high_title_id;
    u32 low_title_id;
    u32 _;
    uint16_t group_id;
    uint16_t field7_0x16;
    uint16_t count; /* count of what??? */
    uint16_t error_count;
    uint16_t dl_frequency;  /* ???????? */
    uint16_t other_dl_freq; /* ???????? */
    int32_t error_code;
    uint8_t subtask_id;
    uint8_t subtask_type;
    uint8_t subtask_flags;
    uint8_t field16_0x27;
    uint32_t subtask_bitmask;
    int unknownOtherValue;
    uint32_t dl_timestamp; /* Last DL time */
    uint32_t subtask_timestamps[32];
    char dl_url[236];
    char filename[64]; /* Null-terminated */
    uint8_t unk6[29];
    uint8_t should_use_rootca;
    uint16_t field25_0x1fe;
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
