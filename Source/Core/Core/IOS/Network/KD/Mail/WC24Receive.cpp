// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/Mail/WC24Receive.h"
#include "Common/Assert.h"
#include "Common/Swap.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Uids.h"

namespace IOS::HLE::NWC24::Mail
{
constexpr char RECEIVE_LIST_PATH[] = "/" WII_WC24CONF_DIR "/mbox"
                                     "/wc24recv.ctl";

WC24ReceiveList::WC24ReceiveList(std::shared_ptr<FS::FileSystem> fs) : m_fs{std::move(fs)}
{
  if (!ReadReceiveList())
  {
    ERROR_LOG_FMT(IOS_WC24, "There is an error in the Receive List for WC24 mail. Mail will be "
                            "unavailable for this IOS session.");
    m_is_disabled = true;

    // If the Send list is corrupted, delete it.
    const FS::ResultCode result = m_fs->Delete(PID_KD, PID_KD, RECEIVE_LIST_PATH);
    if (result != FS::ResultCode::Success && result != FS::ResultCode::NotFound)
    {
      ERROR_LOG_FMT(IOS_WC24, "Failed to delete the Receive list.");
    }
  }
}

bool WC24ReceiveList::ReadReceiveList()
{
  const auto file = m_fs->OpenFile(PID_KD, PID_KD, RECEIVE_LIST_PATH, FS::Mode::Read);
  if (!file || !file->Read(&m_data, 1))
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to read the Receive list");
    return false;
  }

  if (!CheckReceiveList())
  {
    ERROR_LOG_FMT(IOS_WC24, "There is an error in the Receive List for WC24 mail");
    return false;
  }

  return true;
}

void WC24ReceiveList::WriteReceiveList() const
{
  ASSERT(!IsDisabled());
  constexpr FS::Modes public_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::ReadWrite};
  m_fs->CreateFullPath(PID_KD, PID_KD, RECEIVE_LIST_PATH, 0, public_modes);
  const auto file = m_fs->CreateAndOpenFile(PID_KD, PID_KD, RECEIVE_LIST_PATH, public_modes);

  if (!file || !file->Write(&m_data, 1))
    ERROR_LOG_FMT(IOS_WC24, "Failed to open or write WC24 Receive list file");
}

bool WC24ReceiveList::CheckReceiveList() const
{
  // 'WcTF' magic
  if (Common::swap32(m_data.header.magic) != MAIL_LIST_MAGIC)
  {
    ERROR_LOG_FMT(IOS_WC24, "Receive List magic mismatch ({} != {})",
                  Common::swap32(m_data.header.magic), MAIL_LIST_MAGIC);
    return false;
  }

  if (Common::swap32(m_data.header.version) != 4)
  {
    ERROR_LOG_FMT(IOS_WC24, "Receive List version mismatch");
    return false;
  }

  return true;
}

bool WC24ReceiveList::IsDisabled() const
{
  return m_is_disabled;
}

u32 WC24ReceiveList::GetNextEntryId() const
{
  ASSERT(!IsDisabled());
  return Common::swap32(m_data.header.next_entry_id);
}

u32 WC24ReceiveList::GetNextEntryIndex() const
{
  ASSERT(!IsDisabled());
  return (Common::swap32(m_data.header.next_entry_offset) - 128) / 128;
}

void WC24ReceiveList::InitFlag(u32 index)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].flag = 0x200200;
}

void WC24ReceiveList::FinalizeEntry(u32 index)
{
  ASSERT(!IsDisabled());
  u32 next_entry_index = UINT32_MAX;
  for (u32 i = 0; i < MAX_ENTRIES; i++)
  {
    if (m_data.entries[i].id == 0)
    {
      next_entry_index = i;
      break;
    }
  }

  if (next_entry_index == UINT32_MAX)
  {
    // If the file is full, it will clear the first entry.
    ClearEntry(0);
    next_entry_index = 0;
  }

  m_data.entries[index].flag = Common::swap32(m_data.entries[index].flag | 0x220);
  m_data.entries[index].always_0x80000000 = Common::swap32(0x80000000);
  m_data.header.next_entry_offset = CalculateFileOffset(next_entry_index);
  m_data.header.number_of_mail = Common::swap32(Common::swap32(m_data.header.number_of_mail) + 1);
  m_data.header.next_entry_id = Common::swap32(Common::swap32(m_data.header.next_entry_id) + 1);
}

void WC24ReceiveList::ClearEntry(u32 index)
{
  ASSERT(!IsDisabled());
  std::memset(&m_data.entries[index], 0, sizeof(MailListEntry));
}

u32 WC24ReceiveList::GetAppID(u32 index) const
{
  ASSERT(!IsDisabled());
  return Common::swap32(m_data.entries[index].app_id);
}

u16 WC24ReceiveList::GetAppGroup(u32 index) const
{
  ASSERT(!IsDisabled());
  return Common::swap16(m_data.entries[index].app_group);
}

u32 WC24ReceiveList::GetWiiCmd(u32 index) const
{
  ASSERT(!IsDisabled());
  return Common::swap32(m_data.entries[index].wii_cmd);
}

void WC24ReceiveList::UpdateFlag(u32 index, u32 value, FlagOP op)
{
  ASSERT(!IsDisabled());
  if (op == FlagOP::Or)
    m_data.entries[index].flag |= value;
  else
    m_data.entries[index].flag &= value;
}

void WC24ReceiveList::SetMessageId(u32 index, u32 id)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].id = Common::swap32(id);
}

void WC24ReceiveList::SetMessageSize(u32 index, u32 size)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].msg_size = Common::swap32(size);
}

void WC24ReceiveList::SetHeaderLength(u32 index, u32 size)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].header_length = Common::swap32(size);
}

void WC24ReceiveList::SetMessageOffset(u32 index, u32 offset)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].message_offset = Common::swap32(offset);
}

void WC24ReceiveList::SetEncodedMessageLength(u32 index, u32 size)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].encoded_length = Common::swap32(size);
}

void WC24ReceiveList::SetMessageLength(u32 index, u32 size)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].message_length = Common::swap32(size);
}

void WC24ReceiveList::SetPackedContentTransferEncoding(u32 index, u32 offset, u32 size)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].packed_transfer_encoding = Common::swap32(PackData(offset, size));
}

void WC24ReceiveList::SetPackedCharset(u32 index, u32 offset, u32 size)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].packed_charset = Common::swap32(PackData(offset, size));
}

void WC24ReceiveList::SetTime(u32 index, u32 time)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].minutes_since_1900 = Common::swap32(time);
}

void WC24ReceiveList::SetFromFriendCode(u32 index, u64 friend_code)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].from_friend_code = Common::swap64(friend_code);
}

void WC24ReceiveList::SetPackedFrom(u32 index, u32 offset, u32 size)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].packed_from = Common::swap32(PackData(offset, size));
}

void WC24ReceiveList::SetPackedSubject(u32 index, u32 offset, u32 size)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].packed_subject = Common::swap32(PackData(offset, size));
}

void WC24ReceiveList::SetPackedTo(u32 index, u32 offset, u32 size)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].packed_to = Common::swap32(PackData(offset, size));
}

void WC24ReceiveList::SetWiiAppId(u32 index, u32 id)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].app_id = Common::swap32(id);
}

void WC24ReceiveList::SetWiiAppGroupId(u32 index, u16 id)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].app_group = Common::swap16(id);
}

void WC24ReceiveList::SetWiiCmd(u32 index, u32 cmd)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].wii_cmd = Common::swap32(cmd);
}

void WC24ReceiveList::SetNumberOfRecipients(u32 index, u8 num)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].number_of_recipients = num;
}

void WC24ReceiveList::SetMultipartField(u32 index, u32 multipart_index, u32 offset, u32 size)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].multipart_entries[multipart_index] =
      MultipartEntry{Common::swap32(offset), Common::swap32(size)};
  m_data.entries[index].number_of_multipart_entries++;
}

void WC24ReceiveList::SetMultipartContentType(u32 index, u32 multipart_index, u32 type)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].multipart_content_types[multipart_index] = Common::swap32(type);
}

void WC24ReceiveList::SetMultipartSize(u32 index, u32 multipart_index, u32 size)
{
  ASSERT(!IsDisabled());
  m_data.entries[index].multipart_sizes[multipart_index] = Common::swap32(size);
}

std::string WC24ReceiveList::GetMailPath(const u32 index)
{
  return fmt::format("mb/r{:07d}.msg", index);
}
}  // namespace IOS::HLE::NWC24::Mail
