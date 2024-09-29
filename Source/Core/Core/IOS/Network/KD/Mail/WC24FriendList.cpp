// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/Mail/WC24FriendList.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Network/KD/Mail/MailCommon.h"
#include "Core/IOS/Uids.h"

namespace IOS::HLE::NWC24::Mail
{
WC24FriendList::WC24FriendList(std::shared_ptr<FS::FileSystem> fs) : m_fs{std::move(fs)}
{
  ReadFriendList();
}

void WC24FriendList::ReadFriendList()
{
  const auto file = m_fs->OpenFile(PID_KD, PID_KD, FRIEND_LIST_PATH, FS::Mode::Read);
  if (!file || !file->Read(&m_data, 1))
    return;

  const bool success = CheckFriendList();
  if (!success)
    ERROR_LOG_FMT(IOS_WC24, "There is an error in the Receive List for WC24 mail");
}

void WC24FriendList::WriteFriendList() const
{
  constexpr FS::Modes public_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::ReadWrite};
  m_fs->CreateFullPath(PID_KD, PID_KD, FRIEND_LIST_PATH, 0, public_modes);
  const auto file = m_fs->CreateAndOpenFile(PID_KD, PID_KD, FRIEND_LIST_PATH, public_modes);

  if (!file || !file->Write(&m_data, 1))
    ERROR_LOG_FMT(IOS_WC24, "Failed to open or write WC24 Receive list file");
}

bool WC24FriendList::CheckFriendList() const
{
  // 'WcFl' magic
  if (Common::swap32(m_data.header.magic) != FRIEND_LIST_MAGIC)
  {
    ERROR_LOG_FMT(IOS_WC24, "Receive List magic mismatch ({} != {})",
                  Common::swap32(m_data.header.magic), FRIEND_LIST_MAGIC);
    return false;
  }

  return true;
}

bool WC24FriendList::IsFriend(u64 friend_id) const
{
  return std::any_of(m_data.friend_codes.cbegin(), m_data.friend_codes.cend(),
                     [&friend_id](const u64 v) { return v == friend_id; });
}

bool WC24FriendList::IsFriendEstablished(u64 code) const
{
  if (code == Common::swap64(NINTENDO_FRIEND_CODE))
    return true;

  for (u32 i = 0; i < MAX_ENTRIES; i++)
  {
    if (Common::swap64(m_data.friend_codes[i]) == code)
    {
      return m_data.entries[i].status == Common::swap32(static_cast<u32>(FriendStatus::Confirmed));
    }
  }

  return false;
}

std::vector<u64> WC24FriendList::GetUnconfirmedFriends() const
{
  std::vector<u64> friends{};
  for (u32 i = 0; i < MAX_ENTRIES; i++)
  {
    if (static_cast<FriendStatus>(Common::swap32(m_data.entries[i].status)) ==
            FriendStatus::Unconfirmed &&
        static_cast<FriendType>(Common::swap32(m_data.entries[i].friend_type)) == FriendType::Wii)
    {
      friends.push_back(Common::swap64(m_data.friend_codes.at(i)));
    }
  }

  return friends;
}

u64 WC24FriendList::ConvertEmailToFriendCode(std::string_view email)
{
  u32 upper = 0x80;
  u32 lower{};

  u32 idx{};
  for (char chr : email)
  {
    if (idx == 7)
    {
      upper = upper | (email.size() & 0x1f);
      break;
    }

    lower = (upper | chr) >> 0x18 | (lower | lower >> 0x1f) << 8;
    upper = (upper | chr) * 0x100;
    idx++;
  }

  return u64{lower} << 32 | upper;
}

void WC24FriendList::SetFriendStatus(u64 code, FriendStatus status)
{
  for (u32 i = 0; i < MAX_ENTRIES; i++)
  {
    if (Common::swap64(m_data.friend_codes[i]) == code)
    {
      m_data.entries[i].status = Common::swap32(static_cast<u32>(status));
      return;
    }
  }
}
}  // namespace IOS::HLE::NWC24::Mail
