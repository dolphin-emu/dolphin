// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IPC.h"

class PointerWrap;
class SysConf;

namespace IOS
{
namespace HLE
{
void BackUpBTInfoSection(SysConf* sysconf);
void RestoreBTInfoSection(SysConf* sysconf);

namespace Device
{
class BluetoothBase : public Device
{
public:
  BluetoothBase(u32 device_id, const std::string& device_name) : Device(device_id, device_name) {}
  virtual void UpdateSyncButtonState(bool is_held) {}
  virtual void TriggerSyncButtonPressedEvent() {}
  virtual void TriggerSyncButtonHeldEvent() {}
protected:
  static constexpr int ACL_PKT_SIZE = 339;
  static constexpr int ACL_PKT_NUM = 10;
  static constexpr int SCO_PKT_SIZE = 64;
  static constexpr int SCO_PKT_NUM = 0;

  enum USBIOCtl
  {
    USBV0_IOCTL_CTRLMSG = 0,
    USBV0_IOCTL_BLKMSG = 1,
    USBV0_IOCTL_INTRMSG = 2,
  };

  enum USBEndpoint
  {
    HCI_CTRL = 0x00,
    HCI_EVENT = 0x81,
    ACL_DATA_IN = 0x82,
    ACL_DATA_OUT = 0x02
  };

  struct CtrlMessage
  {
    CtrlMessage(const IOCtlVRequest& ioctlv);
    IOCtlVRequest ios_request;
    u8 request_type = 0;
    u8 request = 0;
    u16 value = 0;
    u16 index = 0;
    u16 length = 0;
    u32 payload_addr = 0;
  };

  struct CtrlBuffer
  {
    CtrlBuffer(const IOCtlVRequest& ioctlv);
    IOCtlVRequest ios_request;
    void FillBuffer(const u8* src, size_t size) const;
    u8 m_endpoint = 0;
    u16 m_length = 0;
    u32 m_payload_addr = 0;
  };
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
