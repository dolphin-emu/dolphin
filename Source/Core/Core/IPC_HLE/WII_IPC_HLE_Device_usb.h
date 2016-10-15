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

enum USBV0IOCtl
{
  USBV0_IOCTL_CTRLMSG = 0,
  USBV0_IOCTL_BLKMSG = 1,
  USBV0_IOCTL_INTRMSG = 2,
  USBV0_IOCTL_SUSPENDDEV = 5,
  USBV0_IOCTL_RESUMEDEV = 6,
  USBV0_IOCTL_ISOMSG = 9,
  USBV0_IOCTL_GETDEVLIST = 12,
  USBV0_IOCTL_DEVREMOVALHOOK = 26,
  USBV0_IOCTL_DEVINSERTHOOK = 27,
  USBV0_IOCTL_DEVICECLASSCHANGE = 28,
};

enum StandardDeviceRequestCodes
{
  REQUEST_SET_CONFIGURATION = 9,
  REQUEST_GET_INTERFACE = 10,
  REQUEST_SET_INTERFACE = 11,
};

enum ControlRequestTypes
{
  USB_DIR_HOST2DEVICE = 0,
  USB_DIR_DEVICE2HOST = 1,
  USB_TYPE_STANDARD = 0,
  USB_TYPE_VENDOR = 2,
  USB_REC_DEVICE = 0,
  USB_REC_INTERFACE = 1,
};

constexpr u16 USBHDR(u8 dir, u8 type, u8 recipient, u8 request)
{
  return ((dir << 7 | type << 5 | recipient) << 8) | request;
}

// Base class for USBV0 devices
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

  class IsoMessageBuffer
  {
  public:
    IsoMessageBuffer() = default;
    IsoMessageBuffer(const SIOCtlVBuffer& cmd_buffer, u32 command_address);
    u8 m_endpoint;
    u16 m_length;
    u8 m_num_packets;
    u16* m_packet_sizes;
    u8* m_packets;
    u32 m_cmd_address;
  };
};
