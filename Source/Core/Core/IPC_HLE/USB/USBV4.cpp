// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/USB/USBV4.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

// Source: https://wiibrew.org/w/index.php?title=/dev/usb/hid&oldid=96809
#pragma pack(push, 1)
struct HIDRequest
{
  u8 padding[16];
  s32 device_no;
  union {
    struct
    {
      u8 bmRequestType;
      u8 bmRequest;
      u16 wValue;
      u16 wIndex;
      u16 wLength;
    } control;
    struct
    {
      u32 endpoint;
      u32 length;
    } interrupt;
    struct
    {
      u8 bIndex;
    } string;
  };
  u32 data_addr;
};
#pragma pack(pop)

USBV4Request::USBV4Request(const IOCtlBuffer& cmd_buffer)
{
  cmd_address = cmd_buffer.m_address;
  const auto* req = reinterpret_cast<HIDRequest*>(Memory::GetPointer(cmd_buffer.m_in_buffer_addr));
  device_id = Common::swap32(req->device_no);
}

USBV4CtrlMessage::USBV4CtrlMessage(const IOCtlBuffer& cmd_buffer)
{
  cmd_address = cmd_buffer.m_address;
  const auto* req = reinterpret_cast<HIDRequest*>(Memory::GetPointer(cmd_buffer.m_in_buffer_addr));
  device_id = Common::swap32(req->device_no);
  bmRequestType = req->control.bmRequestType;
  bmRequest = req->control.bmRequest;
  wValue = Common::swap16(req->control.wValue);
  wIndex = Common::swap16(req->control.wIndex);
  wLength = Common::swap16(req->control.wLength);
  data_addr = Common::swap32(req->data_addr);
}

USBV4GetUSStringMessage::USBV4GetUSStringMessage(const IOCtlBuffer& cmd_buffer)
{
  cmd_address = cmd_buffer.m_address;
  const auto* req = reinterpret_cast<HIDRequest*>(Memory::GetPointer(cmd_buffer.m_in_buffer_addr));
  device_id = Common::swap32(req->device_no);
  bmRequestType = 0x80;
  bmRequest = REQUEST_GET_DESCRIPTOR;
  wValue = (0x03 << 8) | req->string.bIndex;
  wIndex = 0x0409;  // language US
  wLength = 255;
  data_addr = Common::swap32(req->data_addr);
}

USBV4IntrMessage::USBV4IntrMessage(const IOCtlBuffer& cmd_buffer)
{
  cmd_address = cmd_buffer.m_address;
  const auto* req = reinterpret_cast<HIDRequest*>(Memory::GetPointer(cmd_buffer.m_in_buffer_addr));
  device_id = Common::swap32(req->device_no);
  data_addr = Common::swap32(req->data_addr);
  length = static_cast<u16>(Common::swap32(req->interrupt.length));
  endpoint = static_cast<u8>(Common::swap32(req->interrupt.endpoint));
}
