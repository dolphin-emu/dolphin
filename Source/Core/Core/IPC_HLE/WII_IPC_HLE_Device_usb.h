// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class PointerWrap;

// Base class for USB devices
class CWII_IPC_HLE_Device_usb : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_usb(u32 device_id, const std::string& device_name)
      : IWII_IPC_HLE_Device(device_id, device_name)
  {
  }
  virtual ~CWII_IPC_HLE_Device_usb() override = default;

  virtual IPCCommandResult Open(u32 command_address, u32 mode) override = 0;
  virtual IPCCommandResult Close(u32 command_address, bool force) override = 0;
  virtual IPCCommandResult IOCtl(u32 command_address) override = 0;
  virtual IPCCommandResult IOCtlV(u32 command_address) override = 0;

protected:
  enum USBIOCtl
  {
    USBV0_IOCTL_CTRLMSG = 0,
    USBV0_IOCTL_BLKMSG = 1,
    USBV0_IOCTL_INTRMSG = 2,
  };

  struct CtrlMessage
  {
    CtrlMessage() = default;
    CtrlMessage(const SIOCtlVBuffer& cmd_buffer);

    u8 request_type;
    u8 request;
    u16 value;
    u16 index;
    u16 length;
    u32 payload_addr;
    u32 address;
  };

  class CtrlBuffer
  {
  public:
    CtrlBuffer() = default;
    CtrlBuffer(const SIOCtlVBuffer& cmd_buffer, u32 command_address);

    void FillBuffer(const u8* src, size_t size) const;
    void SetRetVal(const u32 retval) const { Memory::Write_U32(retval, m_cmd_address + 4); }
    bool IsValid() const { return m_cmd_address != 0; }
    void Invalidate() { m_cmd_address = m_payload_addr = 0; }
    u8 m_endpoint;
    u16 m_length;
    u32 m_payload_addr;
    u32 m_cmd_address;
  };
};
