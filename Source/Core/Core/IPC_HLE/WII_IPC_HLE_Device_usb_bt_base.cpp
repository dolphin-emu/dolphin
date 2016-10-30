// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_base.h"

constexpr u16 BT_INFO_SECTION_LENGTH = 0x460;

void BackUpBTInfoSection(SysConf* sysconf)
{
  const std::string filename = File::GetUserPath(D_SESSION_WIIROOT_IDX) + DIR_SEP WII_BTDINF_BACKUP;
  if (File::Exists(filename))
    return;
  File::IOFile backup(filename, "wb");
  std::vector<u8> section(BT_INFO_SECTION_LENGTH);
  if (!sysconf->GetArrayData("BT.DINF", section.data(), static_cast<u16>(section.size())))
  {
    ERROR_LOG(WII_IPC_WIIMOTE, "Failed to read source BT.DINF section");
    return;
  }
  if (!backup.WriteBytes(section.data(), section.size()))
    ERROR_LOG(WII_IPC_WIIMOTE, "Failed to back up BT.DINF section");
}

void RestoreBTInfoSection(SysConf* sysconf)
{
  const std::string filename = File::GetUserPath(D_SESSION_WIIROOT_IDX) + DIR_SEP WII_BTDINF_BACKUP;
  if (!File::Exists(filename))
    return;
  File::IOFile backup(filename, "rb");
  std::vector<u8> section(BT_INFO_SECTION_LENGTH);
  if (!backup.ReadBytes(section.data(), section.size()))
  {
    ERROR_LOG(WII_IPC_WIIMOTE, "Failed to read backed up BT.DINF section");
    return;
  }
  sysconf->SetArrayData("BT.DINF", section.data(), static_cast<u16>(section.size()));
  File::Delete(filename);
}

IPCCommandResult CWII_IPC_HLE_Device_usb_oh1_57e_305_base::IOCtl(u32 command_address)
{
  // NeoGamma (homebrew) is known to use this path.
  ERROR_LOG(WII_IPC_WIIMOTE, "Bad IOCtl to /dev/usb/oh1/57e/305");
  Memory::Write_U32(FS_EINVAL, command_address + 4);
  return GetDefaultReply();
}

CWII_IPC_HLE_Device_usb_oh1_57e_305_base::CtrlMessage::CtrlMessage(const SIOCtlVBuffer& cmd_buffer)
{
  request_type = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
  request = Memory::Read_U8(cmd_buffer.InBuffer[1].m_Address);
  value = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[2].m_Address));
  index = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[3].m_Address));
  length = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[4].m_Address));
  payload_addr = cmd_buffer.PayloadBuffer[0].m_Address;
  address = cmd_buffer.m_Address;
}

CWII_IPC_HLE_Device_usb_oh1_57e_305_base::CtrlBuffer::CtrlBuffer(const SIOCtlVBuffer& cmd_buffer,
                                                                 const u32 command_address)
{
  m_endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
  m_length = Memory::Read_U16(cmd_buffer.InBuffer[1].m_Address);
  m_payload_addr = cmd_buffer.PayloadBuffer[0].m_Address;
  m_cmd_address = command_address;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_base::CtrlBuffer::FillBuffer(const u8* src,
                                                                      const size_t size) const
{
  _dbg_assert_msg_(WII_IPC_WIIMOTE, size <= m_length, "FillBuffer: size %li > payload length %i",
                   size, m_length);
  Memory::CopyToEmu(m_payload_addr, src, size);
}
