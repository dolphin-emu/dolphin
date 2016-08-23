// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/StringUtil.h"

#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_ven.h"

CWII_IPC_HLE_Device_usb_ven::CWII_IPC_HLE_Device_usb_ven(u32 _DeviceID,
                                                         const std::string& _rDeviceName)
    : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
}

CWII_IPC_HLE_Device_usb_ven::~CWII_IPC_HLE_Device_usb_ven()
{
}

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::Open(u32 _CommandAddress, u32 _Mode)
{
  Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::Close(u32 _CommandAddress, bool _bForce)
{
  if (!_bForce)
    Memory::Write_U32(0, _CommandAddress + 4);

  m_Active = false;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::IOCtlV(u32 _CommandAddress)
{
  SIOCtlVBuffer CommandBuffer(_CommandAddress);

  DEBUG_LOG(OSHLE, "%s - IOCtlV:", GetDeviceName().c_str());
  DEBUG_LOG(OSHLE, "  Parameter: 0x%x", CommandBuffer.Parameter);
  DEBUG_LOG(OSHLE, "  NumberIn: 0x%08x", CommandBuffer.NumberInBuffer);
  DEBUG_LOG(OSHLE, "  NumberOut: 0x%08x", CommandBuffer.NumberPayloadBuffer);
  DEBUG_LOG(OSHLE, "  BufferVector: 0x%08x", CommandBuffer.BufferVector);
  DumpAsync(CommandBuffer.BufferVector, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);

  Memory::Write_U32(0, _CommandAddress + 4);
  return GetNoReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_ven::IOCtl(u32 _CommandAddress)
{
  IPCCommandResult Reply = GetNoReply();
  u32 Command = Memory::Read_U32(_CommandAddress + 0x0c);
  u32 BufferIn = Memory::Read_U32(_CommandAddress + 0x10);
  u32 BufferInSize = Memory::Read_U32(_CommandAddress + 0x14);
  u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
  u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1c);

  DEBUG_LOG(OSHLE, "%s - IOCtl: %x", GetDeviceName().c_str(), Command);
  DEBUG_LOG(OSHLE, "%x:%x %x:%x", BufferIn, BufferInSize, BufferOut, BufferOutSize);

  switch (Command)
  {
    case USBV5_IOCTL_GETVERSION:
      Memory::Write_U32(0x50001, BufferOut);
      Reply = GetDefaultReply();
    break;

    case USBV5_IOCTL_GETDEVICECHANGE:
    {
      // sent on change
      static bool firstcall = true;
	  if (firstcall)
      {
        Reply = GetDefaultReply();
        firstcall = false;
      }

      // num devices
      Memory::Write_U32(0, _CommandAddress + 4);
      return Reply;
    }
    break;

    case USBV5_IOCTL_ATTACHFINISH:
      Reply = GetDefaultReply();
    break;

    case USBV5_IOCTL_SUSPEND_RESUME:
      DEBUG_LOG(OSHLE, "Device: %i Resumed: %i", Memory::Read_U32(BufferIn), Memory::Read_U32(BufferIn + 4));
      Reply = GetDefaultReply();
    break;

    case USBV5_IOCTL_GETDEVPARAMS:
    {
      s32 device = Memory::Read_U32(BufferIn);
      u32 unk = Memory::Read_U32(BufferIn + 4);

      DEBUG_LOG(OSHLE, "USBV5_IOCTL_GETDEVPARAMS device: %i unk: %i", device, unk);

      Memory::Write_U32(0, BufferOut);

      Reply = GetDefaultReply();
    }
    break;

    default:
      DEBUG_LOG(OSHLE, "%x:%x %x:%x", BufferIn, BufferInSize, BufferOut, BufferOutSize);
    break;
  }

  Memory::Write_U32(0, _CommandAddress + 4);
  return Reply;
}

u32 CWII_IPC_HLE_Device_usb_ven::Update()
{
	return IWII_IPC_HLE_Device::Update();
}

void CWII_IPC_HLE_Device_usb_ven::DoState(PointerWrap &p)
{
}
