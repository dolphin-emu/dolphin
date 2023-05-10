// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/Mail/WC24FriendList.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Uids.h"

namespace IOS::HLE::NWC24
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

  const s32 file_error = CheckFriendList();
  if (!file_error)
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

bool WC24FriendList::CheckFriend(u64 friend_id) const
{
  return std::any_of(m_data.friend_codes.cbegin(), m_data.friend_codes.cend(),
                     [&friend_id](const u64 v) { return v == friend_id; });
}

}  // namespace IOS::HLE::NWC24
