// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/Mail/WC24Send.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fmt/chrono.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Network/KD/VFF/VFFUtil.h"
#include "Core/IOS/Uids.h"

namespace IOS::HLE::NWC24::Mail
{
constexpr const char SEND_LIST_PATH[] = "/" WII_WC24CONF_DIR "/mbox"
                                        "/wc24send.ctl";

WC24SendList::WC24SendList(std::shared_ptr<FS::FileSystem> fs) : m_fs{std::move(fs)}
{
  if (!ReadSendList())
  {
    ERROR_LOG_FMT(IOS_WC24, "There is an error in the Send List for WC24 mail. Mail will be "
                            "unavailable for this IOS session.");
    m_is_disabled = true;

    // If the Send list is corrupted, delete it.
    const FS::ResultCode result = m_fs->Delete(PID_KD, PID_KD, SEND_LIST_PATH);
    if (result != FS::ResultCode::Success && result != FS::ResultCode::NotFound)
    {
      ERROR_LOG_FMT(IOS_WC24, "Failed to delete the Send list.");
    }
  }
}

bool WC24SendList::ReadSendList()
{
  const auto file = m_fs->OpenFile(PID_KD, PID_KD, SEND_LIST_PATH, FS::Mode::Read);
  if (!file || !file->Read(&m_data, 1))
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to read the Send list");
    return false;
  }

  if (file->GetStatus()->size != SEND_LIST_SIZE)
  {
    ERROR_LOG_FMT(IOS_WC24, "The WC24 Send list file is not the correct size.");
    return false;
  }

  // Make sure that next_entry_offset is not out of bounds.
  if (m_data.header.next_entry_offset % 128 != 0 ||
      m_data.header.next_entry_offset >
          sizeof(MailListEntry) * (MAX_ENTRIES - 1) + sizeof(MailListEntry))
  {
    const std::optional<u32> next_entry_index = GetNextFreeEntryIndex();
    if (!next_entry_index)
    {
      // If there are no free entries, we will have to overwrite an entry.
      m_data.header.next_entry_offset = Common::swap32(128);
    }
    else
    {
      m_data.header.next_entry_offset = CalculateFileOffset(next_entry_index.value());
    }
  }

  const s32 file_error = CheckSendList();
  if (!file_error)
    ERROR_LOG_FMT(IOS_WC24, "There is an error in the Send List for WC24 mail");

  return true;
}

bool WC24SendList::IsDisabled() const
{
  return m_is_disabled;
}

void WC24SendList::WriteSendList() const
{
  ASSERT(!IsDisabled());
  constexpr FS::Modes public_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::ReadWrite};
  m_fs->CreateFullPath(PID_KD, PID_KD, SEND_LIST_PATH, 0, public_modes);
  const auto file = m_fs->CreateAndOpenFile(PID_KD, PID_KD, SEND_LIST_PATH, public_modes);

  if (!file || !file->Write(&m_data, 1))
    ERROR_LOG_FMT(IOS_WC24, "Failed to open or write WC24 Send list file");
}

bool WC24SendList::CheckSendList() const
{
  // 'WcTf' magic
  if (Common::swap32(m_data.header.magic) != MAIL_LIST_MAGIC)
  {
    ERROR_LOG_FMT(IOS_WC24, "Send List magic mismatch ({} != {})",
                  Common::swap32(m_data.header.magic), MAIL_LIST_MAGIC);
    return false;
  }

  if (Common::swap32(m_data.header.version) != 4)
  {
    ERROR_LOG_FMT(IOS_WC24, "Send List version mismatch");
    return false;
  }

  return true;
}

u32 WC24SendList::GetNumberOfMail() const
{
  ASSERT(!IsDisabled());
  return Common::swap32(m_data.header.number_of_mail);
}

u32 WC24SendList::GetEntryId(u32 entry_index) const
{
  ASSERT(!IsDisabled());
  return Common::swap32(m_data.entries[entry_index].id);
}

u32 WC24SendList::GetMailSize(u32 index) const
{
  ASSERT(!IsDisabled());
  return Common::swap32(m_data.entries[index].msg_size);
}

ErrorCode WC24SendList::DeleteMessage(u32 index)
{
  ASSERT(!IsDisabled());
  ErrorCode error = NWC24::DeleteFileFromVFF(NWC24::Mail::SEND_BOX_PATH, GetMailPath(index), m_fs);
  if (error != WC24_OK)
    return error;

  // Fix up the header then clear the entry.
  m_data.header.number_of_mail = Common::swap32(Common::swap32(m_data.header.number_of_mail) - 1);
  m_data.header.next_entry_id = Common::swap32(GetEntryId(index));
  m_data.header.next_entry_offset = CalculateFileOffset(index);
  m_data.header.total_size_of_messages =
      Common::swap32(m_data.header.total_size_of_messages) - GetMailSize(index);

  std::memset(&m_data.entries[index], 0, sizeof(MailListEntry));
  return WC24_OK;
}

std::string WC24SendList::GetMailPath(u32 index) const
{
  return fmt::format("mb/s{:07d}.msg", GetEntryId(index));
}

u32 WC24SendList::GetNextEntryId() const
{
  ASSERT(!IsDisabled());
  return Common::swap32(m_data.header.next_entry_id);
}

u32 WC24SendList::GetNextEntryIndex() const
{
  ASSERT(!IsDisabled());
  return (Common::swap32(m_data.header.next_entry_offset) - 128) / 128;
}

std::vector<u32> WC24SendList::GetMailToSend() const
{
  ASSERT(!IsDisabled());
  // The list is not guaranteed to have all entries consecutively.
  // As such we must find the populated entries for the specified number of mails.
  const u32 mail_count = std::min(GetNumberOfMail(), 16U);
  u32 found{};

  std::vector<u32> mails{};
  for (u32 index = 0; index < MAX_ENTRIES; index++)
  {
    if (found == mail_count)
      break;

    if (GetEntryId(index) != 0)
    {
      mails.emplace_back(index);
      found++;
    }
  }

  return mails;
}

std::optional<u32> WC24SendList::GetNextFreeEntryIndex() const
{
  for (u32 index = 0; index < MAX_ENTRIES; index++)
  {
    if (GetEntryId(index) == 0)
      return index;
  }

  return std::nullopt;
}

ErrorCode WC24SendList::AddRegistrationMessages(const WC24FriendList& friend_list, u64 sender)
{
  ASSERT(!IsDisabled());
  // It is possible that the user composed a message before SendMail was called.
  ReadSendList();

  const std::vector<u64> unconfirmed_friends = friend_list.GetUnconfirmedFriends();
  for (const u64 code : unconfirmed_friends)
  {
    const u32 entry_index = GetNextEntryIndex();
    const u32 msg_id = GetNextEntryId();
    m_data.entries[entry_index].id = Common::swap32(msg_id);

    const std::time_t t = std::time(nullptr);

    const std::string formatted_message =
        fmt::format(MAIL_REGISTRATION_STRING, sender, code, fmt::gmtime(t));
    const std::span message{reinterpret_cast<const u8*>(formatted_message.data()),
                            formatted_message.size()};
    const ErrorCode reply = WriteToVFF(SEND_BOX_PATH, GetMailPath(entry_index), m_fs, message);

    if (reply != WC24_OK)
    {
      ERROR_LOG_FMT(IOS_WC24, "Error writing registration message to VFF");
      return reply;
    }

    NOTICE_LOG_FMT(IOS_WC24, "Issued registration message for Wii Friend: {}", code);

    // Update the header and some fields in the body
    m_data.entries[entry_index].msg_size = Common::swap32(static_cast<u32>(message.size()));
    m_data.header.number_of_mail = Common::swap32(GetNumberOfMail() + 1);
    m_data.header.next_entry_id = Common::swap32(msg_id + 1);
    m_data.header.total_size_of_messages =
        Common::swap32(m_data.header.total_size_of_messages) + static_cast<u32>(message.size());

    const std::optional<u32> next_entry_index = GetNextFreeEntryIndex();
    if (!next_entry_index)
    {
      // If there are no free entries, we overwrite the first entry.
      m_data.header.next_entry_offset = Common::swap32(128);
    }
    else
    {
      m_data.header.next_entry_offset = CalculateFileOffset(next_entry_index.value());
    }
  }

  // Only flush on success.
  WriteSendList();
  return WC24_OK;
}

std::string_view WC24SendList::GetMailFlag() const
{
  ASSERT(!IsDisabled());
  return {m_data.header.mail_flag.data(), m_data.header.mail_flag.size()};
}
}  // namespace IOS::HLE::NWC24::Mail
