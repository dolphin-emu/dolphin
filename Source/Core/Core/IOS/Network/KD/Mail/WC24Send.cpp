// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/Mail/WC24Send.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Network/KD/VFF/VFFUtil.h"
#include "Core/IOS/Uids.h"

#include <fmt/chrono.h>

namespace IOS::HLE::NWC24::Mail
{
constexpr const char SEND_LIST_PATH[] = "/" WII_WC24CONF_DIR "/mbox"
                                        "/wc24send.ctl";

WC24SendList::WC24SendList(std::shared_ptr<FS::FileSystem> fs) : m_fs{std::move(fs)}
{
  ReadSendList();
}

void WC24SendList::ReadSendList()
{
  const auto file = m_fs->OpenFile(PID_KD, PID_KD, SEND_LIST_PATH, FS::Mode::Read);
  if (!file || !file->Read(&m_data, 1))
    return;

  const s32 file_error = CheckSendList();
  if (!file_error)
    ERROR_LOG_FMT(IOS_WC24, "There is an error in the Send List for WC24 mail");
}

void WC24SendList::WriteSendList() const
{
  constexpr FS::Modes public_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::ReadWrite};
  m_fs->CreateFullPath(PID_KD, PID_KD, SEND_LIST_PATH, 0, public_modes);
  const auto file = m_fs->CreateAndOpenFile(PID_KD, PID_KD, SEND_LIST_PATH, public_modes);

  if (!file || !file->Write(&m_data, 1))
    ERROR_LOG_FMT(IOS_WC24, "Failed to open or write WC24 Send list file");
}

bool WC24SendList::CheckSendList() const
{
  // 'WcTF' magic
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

u32 WC24SendList::GetNextEntryId() const
{
  return Common::swap32(m_data.header.next_entry_id);
}

u32 WC24SendList::GetNextEntryIndex() const
{
  return (Common::swap32(m_data.header.next_entry_offset) - 128) / 128;
}

void WC24SendList::InitEntry(u32 entry_index, u64 from)
{
  m_data.entries[entry_index].flag = 2097409;
  const u32 kd_app_id = Common::swap32(1);
  m_data.entries[entry_index].app_id = kd_app_id;
  m_data.entries[entry_index].app_group = kd_app_id;
  m_data.entries[entry_index].from_friend_code = Common::swap64(from);
  m_data.entries[entry_index].always_0x80000000 = Common::swap32(0x80000000);
}

u32 WC24SendList::GetNumberOfMail() const
{
  return Common::swap32(m_data.header.number_of_mail);
}

void WC24SendList::SetPackedTo(u32 index, u32 offset, u32 size)
{
  m_data.entries[index].packed_to = Common::swap32(PackData(offset, size));
}

std::vector<u32> WC24SendList::GetPopulatedEntries() const
{
  // The list is not guaranteed to have all entries consecutively.
  // As such we must find the populated entries for the specified number of mails.
  const u32 mail_count = std::min(GetNumberOfMail(), 16U);
  u32 found = 0;
  std::vector<u32> mails(mail_count);
  for (u32 i = 0; i < MAX_ENTRIES; i++)
  {
    if (found == mail_count)
      break;

    if (GetIndex(i) < MAX_ENTRIES)
    {
      mails[found] = i;
      found++;
    }
  }

  return mails;
}

std::string_view WC24SendList::GetMailFlag() const
{
  return {m_data.header.mail_flag};
}

u32 WC24SendList::GetMailSize(const u32 index) const
{
  return Common::swap32(m_data.entries[index].msg_size);
}

std::string WC24SendList::GetMailPath(const u32 index) const
{
  return fmt::format("mb/s{:07d}.msg", GetIndex(index));
}

u32 WC24SendList::GetIndex(const u32 index) const
{
  return Common::swap32(m_data.entries[index].id);
}

ErrorCode WC24SendList::DeleteMessage(const u32 index)
{
  // Deleting is broken for the time being.
  // const NWC24::ErrorCode return_code =
  //    NWC24::DeleteFileFromVFF(SEND_BOX_PATH, GetMailPath(index), m_fs);
  m_data.header.number_of_mail = Common::swap32(Common::swap32(m_data.header.number_of_mail) - 1);
  m_data.header.next_entry_offset = Common::swap32(128 + (index * 128));
  m_data.header.next_entry_id = Common::swap32(Common::swap32(m_data.header.next_entry_id) - 1);
  m_data.entries[index].id = 0;
  m_data.entries[index].msg_size = 0;
  return WC24_OK;
}

u32 WC24SendList::GetTotalEntries() const
{
  return Common::swap32(m_data.header.total_entries);
}

ErrorCode WC24SendList::AddRegistrationMessages(const WC24FriendList& friend_list, u64 from)
{
  const std::vector<u64> unconfirmed_friends = friend_list.GetUnconfirmedFriends();
  for (const u64 code : unconfirmed_friends)
  {
    const u32 entry_index = GetNextEntryIndex();
    const u32 msg_id = GetNextEntryId();

    const std::string formatted_message =
        fmt::format(MAIL_REGISTRATION_STRING, from, code, from, code);
    std::vector<u8> message{formatted_message.begin(), formatted_message.end()};
    NWC24::ErrorCode reply =
        NWC24::WriteToVFF(NWC24::Mail::SEND_BOX_PATH, GetMailPath(msg_id), m_fs, message);

    if (reply != WC24_OK)
      ERROR_LOG_FMT(IOS_WC24, "Error writing reg to VFF");

    NOTICE_LOG_FMT(IOS_WC24, "Issued message for {}", code);
    m_data.entries[entry_index].id = Common::swap32(msg_id);
    m_data.header.next_entry_id = Common::swap32(entry_index + 1);
    m_data.header.number_of_mail = Common::swap32(GetNumberOfMail() + 1);
    m_data.header.total_entries = Common::swap32(GetTotalEntries() + 1);
    // The mail that is sent is for the most part constant, so we can assign hardcoded values.
    /*m_data.entries[entry_index].flag = Common::swap32(2097945);
    m_data.entries[entry_index].app_id = 1;
    m_data.entries[entry_index].wii_cmd = Common::swap32(2147549185);*/
  }

  // Flush data to file
  WriteSendList();
  return WC24_OK;
}

std::string WC24SendList::GetUTCTime()
{
  time_t time = std::time(nullptr);
  return fmt::format("{:%d %b %Y %T %z}", fmt::gmtime(time));
}
}  // namespace IOS::HLE::NWC24::Mail
