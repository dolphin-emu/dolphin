// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/Mail/WC24Receive.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Uids.h"

namespace IOS::HLE::NWC24
{
constexpr const char RECEIVE_LIST_PATH[] = "/" WII_WC24CONF_DIR "/mbox"
                                           "/wc24recv.ctl";

WC24ReceiveList::WC24ReceiveList(std::shared_ptr<FS::FileSystem> fs) : m_fs{std::move(fs)}
{
  ReadReceiveList();
}

void WC24ReceiveList::ReadReceiveList()
{
  const auto file = m_fs->OpenFile(PID_KD, PID_KD, RECEIVE_LIST_PATH, FS::Mode::Read);
  if (!file || !file->Read(&m_data, 1))
    return;

  const s32 file_error = CheckReceiveList();
  if (!file_error)
    ERROR_LOG_FMT(IOS_WC24, "There is an error in the Receive List for WC24 mail");
}

void WC24ReceiveList::WriteReceiveList() const
{
  constexpr FS::Modes public_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::ReadWrite};
  m_fs->CreateFullPath(PID_KD, PID_KD, RECEIVE_LIST_PATH, 0, public_modes);
  const auto file = m_fs->CreateAndOpenFile(PID_KD, PID_KD, RECEIVE_LIST_PATH, public_modes);

  if (!file || !file->Write(&m_data, 1))
    ERROR_LOG_FMT(IOS_WC24, "Failed to open or write WC24 Receive list file");
}

bool WC24ReceiveList::CheckReceiveList() const
{
  // 'WcTF' magic
  if (Common::swap32(m_data.header.magic) != RECEIVE_LIST_MAGIC)
  {
    ERROR_LOG_FMT(IOS_WC24, "Receive List magic mismatch ({} != {})",
                  Common::swap32(m_data.header.magic), RECEIVE_LIST_MAGIC);
    return false;
  }

  if (Common::swap32(m_data.header.version) != 4)
  {
    ERROR_LOG_FMT(IOS_WC24, "Receive List version mismatch");
    return false;
  }

  return true;
}

u32 WC24ReceiveList::PackData(u32 one, u32 two)
{
  return (one & 0xFFFFF) | two << 20;
}

u32 WC24ReceiveList::GetNextEntryId() const
{
  // TODO: If it exceeds the max index it will delete the last message. Deleting is currently
  // broken; fix it.
  return Common::swap32(m_data.header.next_entry_id);
}

u32 WC24ReceiveList::GetNextEntryIndex() const
{
  return (Common::swap32(m_data.header.next_entry_offset) - 128) / 128;
}

void WC24ReceiveList::InitFlag(u32 index)
{
  m_data.entries[index].flag = 2097409;
}

void WC24ReceiveList::FinalizeEntry(u32 index)
{
  u32 i{};
  for (; i < MAX_ENTRIES; i++)
  {
    if (m_data.entries[i].flag == 0)
      break;
  }

  m_data.entries[index].flag = Common::swap32(m_data.entries[index].flag | 0x220);
  m_data.entries[index].unk = Common::swap64(0x0001000000010000);
  m_data.entries[index].always_0x80000000 = Common::swap32(0x80000000);
  m_data.header.next_entry_offset = Common::swap32((128 * i) + 128);
  m_data.header.number_of_mail = Common::swap32(Common::swap32(m_data.header.number_of_mail) + 1);
  m_data.header.next_entry_id = Common::swap32(Common::swap32(m_data.header.next_entry_id) + 1);
}

void WC24ReceiveList::ClearEntry(u32 index)
{
  m_data.entries[index].id = 0;
  m_data.entries[index].flag = 0;
  m_data.entries[index].msg_size = 0;
  m_data.entries[index].app_id = 0;
  m_data.entries[index].header_length = 0;
  m_data.entries[index].tag = 0;
  m_data.entries[index].wii_cmd = 0;
  m_data.entries[index].crc32 = 0;
  m_data.entries[index].from_friend_code = 0;
  m_data.entries[index].minutes_since_1900 = 0;
  m_data.entries[index].app_group = 0;
  m_data.entries[index].packed_from = 0;
  m_data.entries[index].packed_to = 0;
  m_data.entries[index].packed_subject = 0;
  m_data.entries[index].packed_charset = 0;
  m_data.entries[index].packed_transfer_encoding = 0;
  m_data.entries[index].message_offset = 0;
  m_data.entries[index].encoded_length = 0;
  m_data.entries[index].message_length = 0;
}

void WC24ReceiveList::UpdateFlag(u32 index, u32 value, FlagOP op)
{
  if (op == OR)
    m_data.entries[index].flag |= value;
  else
    m_data.entries[index].flag &= value;
}

void WC24ReceiveList::SetMessageId(u32 index, u32 id)
{
  m_data.entries[index].id = Common::swap32(id);
}

void WC24ReceiveList::SetMessageSize(u32 index, u32 size)
{
  m_data.entries[index].msg_size = Common::swap32(size);
}

void WC24ReceiveList::SetHeaderLength(u32 index, u32 size)
{
  m_data.entries[index].header_length = Common::swap32(size);
}

void WC24ReceiveList::SetMessageOffset(u32 index, u32 offset)
{
  m_data.entries[index].message_offset = Common::swap32(offset);
}

void WC24ReceiveList::SetEncodedMessageLength(u32 index, u32 size)
{
  m_data.entries[index].encoded_length = Common::swap32(size);
}

void WC24ReceiveList::SetMessageLength(u32 index, u32 size)
{
  m_data.entries[index].message_length = Common::swap32(size);
}

void WC24ReceiveList::SetPackedContentTransferEncoding(u32 index, u32 offset, u32 size)
{
  m_data.entries[index].packed_transfer_encoding = Common::swap32(PackData(offset, size));
}

void WC24ReceiveList::SetPackedCharset(u32 index, u32 offset, u32 size)
{
  m_data.entries[index].packed_charset = Common::swap32(PackData(offset, size));
}

void WC24ReceiveList::SetTime(u32 index, u32 time)
{
  m_data.entries[index].minutes_since_1900 = Common::swap32(time);
}

void WC24ReceiveList::SetFromFriendCode(u32 index, u64 friend_code)
{
  m_data.entries[index].from_friend_code = Common::swap64(friend_code);
}

void WC24ReceiveList::SetPackedFrom(u32 index, u32 offset, u32 size)
{
  m_data.entries[index].packed_from = Common::swap32(PackData(offset, size));
}

void WC24ReceiveList::SetPackedSubject(u32 index, u32 offset, u32 size)
{
  m_data.entries[index].packed_subject = Common::swap32(PackData(offset, size));
}

void WC24ReceiveList::SetPackedTo(u32 index, u32 offset, u32 size)
{
  m_data.entries[index].packed_to = Common::swap32(PackData(offset, size));
}

void WC24ReceiveList::SetWiiAppId(u32 index, u32 id)
{
  m_data.entries[index].app_id = Common::swap32(id);
}

void WC24ReceiveList::SetWiiAppGroupId(u32 index, u32 id)
{
  m_data.entries[index].app_group = Common::swap32(id);
}

void WC24ReceiveList::SetWiiCmd(u32 index, u32 cmd)
{
  m_data.entries[index].wii_cmd = Common::swap32(cmd);
}

std::string WC24ReceiveList::GetMailPath(const u32 index)
{
  return fmt::format("mb/r{:07d}.msg", index);
}
}  // namespace IOS::HLE::NWC24
