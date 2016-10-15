// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb.h"

CWII_IPC_HLE_Device_usb::CtrlMessage::CtrlMessage(const SIOCtlVBuffer& cmd_buffer)
{
  request_type = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
  request = Memory::Read_U8(cmd_buffer.InBuffer[1].m_Address);
  value = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[2].m_Address));
  index = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[3].m_Address));
  length = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[4].m_Address));
  payload_addr = cmd_buffer.PayloadBuffer[0].m_Address;
  address = cmd_buffer.m_Address;
}

CWII_IPC_HLE_Device_usb::CtrlBuffer::CtrlBuffer(const SIOCtlVBuffer& cmd_buffer, const u32 address)
{
  m_endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
  m_length = Memory::Read_U16(cmd_buffer.InBuffer[1].m_Address);
  m_payload_addr = cmd_buffer.PayloadBuffer[0].m_Address;
  m_cmd_address = address;
}

void CWII_IPC_HLE_Device_usb::CtrlBuffer::FillBuffer(const u8* src, const size_t size) const
{
  _dbg_assert_msg_(WII_IPC_USB, size <= m_length, "FillBuffer: size %li > payload length %i", size,
                   m_length);
  Memory::CopyToEmu(m_payload_addr, src, size);
}
