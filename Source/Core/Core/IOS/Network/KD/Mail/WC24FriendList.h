// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
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
constexpr const char FRIEND_LIST_PATH[] = "/" WII_WC24CONF_DIR "/nwc24fl.bin";
class WC24FriendList final
{
public:
  explicit WC24FriendList(std::shared_ptr<FS::FileSystem> fs);
  void ReadFriendList();
  bool CheckFriendList() const;
  void WriteFriendList() const;

  bool CheckFriend(u64 friend_id) const;

private:
  static constexpr u32 FRIEND_LIST_MAGIC = 0x5763466C;  // WcFl
  static constexpr u32 MAX_ENTRIES = 100;

#pragma pack(push, 1)
  struct FriendListHeader final
  {
    u32 magic;  // 'WcFl' 0x5763466C
    u32 version;
    u32 max_friend_entries;
    u32 number_of_friends;
    char padding[48];
  };

  struct FriendListEntry final
  {
    // TODO: Implement this for changing friend status
    char todo[320];
  };

  struct FriendList final
  {
    FriendListHeader header;
    std::array<u64, MAX_ENTRIES> friend_codes;
    FriendListEntry entries[MAX_ENTRIES];
  };
#pragma pack(pop)

  FriendList m_data;
  std::shared_ptr<FS::FileSystem> m_fs;
};
}  // namespace NWC24
}  // namespace IOS::HLE
