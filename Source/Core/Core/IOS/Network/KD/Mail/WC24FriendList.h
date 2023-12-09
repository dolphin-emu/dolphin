// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <string_view>
#include <vector>

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
namespace NWC24::Mail
{
constexpr const char FRIEND_LIST_PATH[] = "/" WII_WC24CONF_DIR "/nwc24fl.bin";
class WC24FriendList final
{
public:
  explicit WC24FriendList(std::shared_ptr<FS::FileSystem> fs);
  static u64 ConvertEmailToFriendCode(std::string_view email);

  void ReadFriendList();
  bool CheckFriendList() const;
  void WriteFriendList() const;

  bool DoesFriendExist(u64 friend_id) const;
  std::vector<u64> GetUnconfirmedFriends() const;

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
  static_assert(sizeof(FriendListHeader) == 64);
  static_assert(std::is_trivially_copyable_v<FriendListHeader>);

  enum class FriendType : u32
  {
    None,
    Wii,
    Email
  };

  enum class FriendStatus : u32
  {
    None,
    Unconfirmed,
    Confirmed,
    Declined
  };

  struct FriendListEntry final
  {
    u32 friend_type;
    u32 status;
    char nickname[24];
    u32 mii_id;
    u32 system_id;
    char reserved[24];
    char email_or_code[96];
    char padding[160];
  };
  static_assert(sizeof(FriendListEntry) == 320);
  static_assert(std::is_trivially_copyable_v<FriendListEntry>);

  struct FriendList final
  {
    FriendListHeader header;
    std::array<u64, MAX_ENTRIES> friend_codes;
    std::array<FriendListEntry, MAX_ENTRIES> entries;
  };
  static_assert(sizeof(FriendList) == 32864);
  static_assert(std::is_trivially_copyable_v<FriendList>);
#pragma pack(pop)

  FriendList m_data;
  std::shared_ptr<FS::FileSystem> m_fs;
};
}  // namespace NWC24::Mail
}  // namespace IOS::HLE
