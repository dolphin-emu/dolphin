// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/Mail/WC24Send.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Uids.h"

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

  if (file->GetStatus()->size != SEND_LIST_SIZE)
  {
    ERROR_LOG_FMT(IOS_WC24, "The WC24 Send list file is not the correct size.");
    return;
  }

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

std::string_view WC24SendList::GetMailFlag() const
{
  return {m_data.header.mail_flag.data(), m_data.header.mail_flag.size()};
}
}  // namespace IOS::HLE::NWC24::Mail
