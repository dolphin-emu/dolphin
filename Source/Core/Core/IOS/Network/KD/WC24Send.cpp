// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/WC24Send.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Network/KD/VFF/VFFUtil.h"
#include "Core/IOS/Uids.h"

namespace IOS::HLE::NWC24
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
  if (Common::swap32(m_data.header.magic) != SEND_LIST_MAGIC)
  {
    ERROR_LOG_FMT(IOS_WC24, "Send List magic mismatch ({} != {})",
                  Common::swap32(m_data.header.magic), SEND_LIST_MAGIC);
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
  return Common::swap32(m_data.header.number_of_mail);
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
  return Common::swap32(m_data.entries[index].mail_size);
}

std::string WC24SendList::GetMailPath(const u32 index) const
{
  return fmt::format("mb/s{:07d}.msg", GetIndex(index));
}

u32 WC24SendList::GetIndex(const u32 index) const
{
  return Common::swap32(m_data.entries[index].index);
}

NWC24::ErrorCode WC24SendList::DeleteMessage(const u32 index)
{
  const NWC24::ErrorCode return_code =
      NWC24::DeleteFileFromVFF(SEND_BOX_PATH, GetMailPath(index), m_fs);
  m_data.header.number_of_mail = Common::swap32(Common::swap32(m_data.header.number_of_mail) - 1);
  m_data.header.next_index_offset = Common::swap32(128 + (index * 128));
  m_data.header.next_index = Common::swap32(Common::swap32(m_data.header.next_index) - 1);
  m_data.entries[index].index = 0;
  m_data.entries[index].mail_size = 0;
  m_data.entries[index].unk1 = 0;
  std::memset(m_data.entries[index]._, 0, 116);
  return return_code;
}
}  // namespace IOS::HLE::NWC24