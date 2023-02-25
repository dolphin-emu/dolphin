// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/NWC24DL.h"

#include "Common/BitUtils.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Uids.h"

namespace IOS::HLE::NWC24
{
constexpr const char DL_LIST_PATH[] = "/" WII_WC24CONF_DIR "/nwc24dl.bin";

NWC24Dl::NWC24Dl(std::shared_ptr<FS::FileSystem> fs) : m_fs{std::move(fs)}
{
  ReadDlList();
}

void NWC24Dl::ReadDlList()
{
  const auto file = m_fs->OpenFile(PID_KD, PID_KD, DL_LIST_PATH, FS::Mode::Read);
  if (!file || !file->Read(&m_data, 1))
    return;

  const s32 file_error = CheckNwc24DlList();
  if (file_error)
    ERROR_LOG_FMT(IOS_WC24, "There is an error in the DL list for WC24: {}", file_error);
}

s32 NWC24Dl::CheckNwc24DlList() const
{
  // 'WcDl' magic
  if (Magic() != DL_LIST_MAGIC)
  {
    ERROR_LOG_FMT(IOS_WC24, "DL list magic mismatch");
    return -1;
  }

  if (Version() != 1)
  {
    ERROR_LOG_FMT(IOS_WC24, "DL list version mismatch");
    return -1;
  }

  return 0;
}

void NWC24Dl::WriteDlList() const
{
  constexpr FS::Modes public_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::ReadWrite};
  m_fs->CreateFullPath(PID_KD, PID_KD, DL_LIST_PATH, 0, public_modes);
  const auto file = m_fs->CreateAndOpenFile(PID_KD, PID_KD, DL_LIST_PATH, public_modes);

  if (!file || !file->Write(&m_data, 1))
    ERROR_LOG_FMT(IOS_WC24, "Failed to open or write WC24 DL list file");
}

bool NWC24Dl::DoesEntryExist(u16 entry_index)
{
  return m_data.entries[entry_index].low_title_id != 0;
}

std::string NWC24Dl::GetDownloadURL(u16 entry_index, std::optional<u8> subtask_id) const
{
  std::string url(m_data.entries[entry_index].dl_url);

  // Determine if we need to append the subtask to the URL.
  if (subtask_id &&
      Common::ExtractBit(Common::swap32(m_data.entries[entry_index].subtask_bitmask), 1))
  {
    url.append(fmt::format(".{:02d}", *subtask_id));
  }

  return url;
}

std::string NWC24Dl::GetVFFContentName(u16 entry_index, std::optional<u8> subtask_id) const
{
  std::string content(m_data.entries[entry_index].filename);

  // Determine if we need to append the subtask to the name.
  if (subtask_id &&
      Common::ExtractBit(Common::swap32(m_data.entries[entry_index].subtask_bitmask), 0))
  {
    content.append(fmt::format(".{:02d}", *subtask_id));
  }

  return content;
}

std::string NWC24Dl::GetVFFPath(u16 entry_index) const
{
  const u32 lower_title_id = Common::swap32(m_data.entries[entry_index].low_title_id);
  const u32 high_title_id = Common::swap32(m_data.entries[entry_index].high_title_id);

  return fmt::format("/title/{0:08x}/{1:08x}/data/wc24dl.vff", lower_title_id, high_title_id);
}

WC24PubkMod NWC24Dl::GetWC24PubkMod(u16 entry_index) const
{
  WC24PubkMod pubkMod;
  const u32 lower_title_id = Common::swap32(m_data.entries[entry_index].low_title_id);
  const u32 high_title_id = Common::swap32(m_data.entries[entry_index].high_title_id);

  const std::string path =
      fmt::format("/title/{0:08x}/{1:08x}/data/wc24pubk.mod", lower_title_id, high_title_id);

  const auto file = m_fs->OpenFile(PID_KD, PID_KD, path, IOS::HLE::FS::Mode::Read);
  file->Read(&pubkMod, 1);

  return pubkMod;
}

bool NWC24Dl::IsEncrypted(u16 entry_index) const
{
  return !!Common::ExtractBit(Common::swap32(m_data.entries[entry_index].flags), 3);
}

u32 NWC24Dl::Magic() const
{
  return Common::swap32(m_data.header.magic);
}

void NWC24Dl::SetMagic(u32 magic)
{
  m_data.header.magic = Common::swap32(magic);
}

u32 NWC24Dl::Version() const
{
  return Common::swap32(m_data.header.version);
}

void NWC24Dl::SetVersion(u32 version)
{
  m_data.header.version = Common::swap32(version);
}

}  // namespace IOS::HLE::NWC24
