// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/USBV4.h"

#include <algorithm>
#include <locale>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Device.h"

namespace IOS
{
namespace HLE
{
namespace USB
{
// Source: https://wiibrew.org/w/index.php?title=/dev/usb/hid&oldid=96809
#pragma pack(push, 1)
struct HIDRequest
{
  u8 padding[16];
  s32 device_no;
  union
  {
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

V4CtrlMessage::V4CtrlMessage(Kernel& ios, const IOCtlRequest& ioctl) : CtrlMessage(ios, ioctl, 0)
{
  HIDRequest hid_request;
  Memory::CopyFromEmu(&hid_request, ioctl.buffer_in, sizeof(hid_request));
  request_type = hid_request.control.bmRequestType;
  request = hid_request.control.bmRequest;
  value = Common::swap16(hid_request.control.wValue);
  index = Common::swap16(hid_request.control.wIndex);
  length = Common::swap16(hid_request.control.wLength);
  data_address = Common::swap32(hid_request.data_addr);
}

// Since this is just a standard control request, but with additional requirements
// (US for the language and replacing non-ASCII characters with '?'),
// we can simply submit it as a usual control request.
V4GetUSStringMessage::V4GetUSStringMessage(Kernel& ios, const IOCtlRequest& ioctl)
    : CtrlMessage(ios, ioctl, 0)
{
  HIDRequest hid_request;
  Memory::CopyFromEmu(&hid_request, ioctl.buffer_in, sizeof(hid_request));
  request_type = 0x80;
  request = REQUEST_GET_DESCRIPTOR;
  value = (0x03 << 8) | hid_request.string.bIndex;
  index = 0x0409;  // language US
  length = 255;
  data_address = Common::swap32(hid_request.data_addr);
}

void V4GetUSStringMessage::OnTransferComplete(s32 return_value) const
{
  const std::locale& c_locale = std::locale::classic();
  std::string message = Memory::GetString(data_address);
  std::replace_if(message.begin(), message.end(),
                  [&c_locale](char c) { return !std::isprint(c, c_locale); }, '?');
  Memory::CopyToEmu(data_address, message.c_str(), message.size());
  TransferCommand::OnTransferComplete(return_value);
}

V4IntrMessage::V4IntrMessage(Kernel& ios, const IOCtlRequest& ioctl) : IntrMessage(ios, ioctl, 0)
{
  HIDRequest hid_request;
  Memory::CopyFromEmu(&hid_request, ioctl.buffer_in, sizeof(hid_request));
  length = static_cast<u16>(Common::swap32(hid_request.interrupt.length));
  endpoint = static_cast<u8>(Common::swap32(hid_request.interrupt.endpoint));
  data_address = Common::swap32(hid_request.data_addr);
}
}  // namespace USB
}  // namespace HLE
}  // namespace IOS
