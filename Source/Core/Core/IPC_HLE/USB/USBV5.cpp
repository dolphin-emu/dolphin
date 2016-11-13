// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <numeric>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/USB/USBV5.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

USBV5TransferCommand::USBV5TransferCommand(const SIOCtlVBuffer& cmd_buffer)
{
  cmd_address = cmd_buffer.m_Address;
  device_id = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address);
}

USBV5CtrlMessage::USBV5CtrlMessage(const SIOCtlVBuffer& cmd_buffer)
{
  cmd_address = cmd_buffer.m_Address;
  device_id = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address);
  bmRequestType = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 8);
  bmRequest = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 9);
  wValue = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address + 10);
  wIndex = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address + 12);
  wLength = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address + 14);
  data_addr = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address + 16);
}

USBV5BulkMessage::USBV5BulkMessage(const SIOCtlVBuffer& cmd_buffer)
{
  cmd_address = cmd_buffer.m_Address;
  device_id = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address);
  data_addr = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address + 8);
  length = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address + 12);
  endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 18);
}

USBV5IntrMessage::USBV5IntrMessage(const SIOCtlVBuffer& cmd_buffer)
{
  cmd_address = cmd_buffer.m_Address;
  device_id = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address);
  data_addr = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address + 8);
  length = Memory::Read_U16(cmd_buffer.InBuffer[0].m_Address + 12);
  endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 14);
}

USBV5IsoMessage::USBV5IsoMessage(const SIOCtlVBuffer& cmd_buffer)
{
  cmd_address = cmd_buffer.m_Address;
  device_id = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address);
  data_addr = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address + 8);
  num_packets = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 16);
  endpoint = Memory::Read_U8(cmd_buffer.InBuffer[0].m_Address + 17);
  packet_sizes_addr = Memory::Read_U32(cmd_buffer.InBuffer[0].m_Address + 12);
  const u16* sizes = reinterpret_cast<const u16*>(Memory::GetPointer(packet_sizes_addr));
  for (int i = 0; i < num_packets; ++i)
    packet_sizes.push_back(Common::swap16(sizes[i]));
  length = std::accumulate(packet_sizes.begin(), packet_sizes.end(), 0);
}
