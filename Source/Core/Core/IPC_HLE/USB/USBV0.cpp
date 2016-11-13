// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/USB/USBV0.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

USBV0CtrlMessage::USBV0CtrlMessage(const SIOCtlVBuffer& cmd_buffer)
{
  data_addr = cmd_buffer.PayloadBuffer[0].m_Address;
  cmd_address = cmd_buffer.m_Address;
  bmRequestType = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
  bmRequest = Memory::Read_U8(cmd_buffer.InBuffer[1].m_Address);
  wValue = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[2].m_Address));
  wIndex = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[3].m_Address));
  wLength = Common::swap16(Memory::Read_U16(cmd_buffer.InBuffer[4].m_Address));
}

USBV0BulkMessage::USBV0BulkMessage(const SIOCtlVBuffer& cmd_buffer)
{
  data_addr = cmd_buffer.PayloadBuffer[0].m_Address;
  cmd_address = cmd_buffer.m_Address;
  endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
  length = Memory::Read_U16(cmd_buffer.InBuffer[1].m_Address);
}

USBV0IntrMessage::USBV0IntrMessage(const SIOCtlVBuffer& cmd_buffer)
{
  data_addr = cmd_buffer.PayloadBuffer[0].m_Address;
  cmd_address = cmd_buffer.m_Address;
  endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
  length = Memory::Read_U16(cmd_buffer.InBuffer[1].m_Address);
}

USBV0IsoMessage::USBV0IsoMessage(const SIOCtlVBuffer& cmd_buffer)
{
  data_addr = cmd_buffer.PayloadBuffer[1].m_Address;
  cmd_address = cmd_buffer.m_Address;
  endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address);
  length = Memory::Read_U16(cmd_buffer.InBuffer[1].m_Address);
  num_packets = Memory::Read_U8(cmd_buffer.InBuffer[2].m_Address);
  packet_sizes_addr = cmd_buffer.PayloadBuffer[0].m_Address;
  const u16* sizes = reinterpret_cast<const u16*>(Memory::GetPointer(packet_sizes_addr));
  for (int i = 0; i < num_packets; ++i)
    packet_sizes.push_back(Common::swap16(sizes[i]));
}
