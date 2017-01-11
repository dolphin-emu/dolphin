// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class PointerWrap;
class SysConf;

void BackUpBTInfoSection(SysConf* sysconf);
void RestoreBTInfoSection(SysConf* sysconf);

class CWII_IPC_HLE_Device_usb_oh1_57e_305_base : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_usb_oh1_57e_305_base(u32 device_id, const std::string& device_name)
      : IWII_IPC_HLE_Device(device_id, device_name)
  {
  }
  virtual ~CWII_IPC_HLE_Device_usb_oh1_57e_305_base() override = default;

  virtual IPCCommandResult Open(u32 command_address, u32 mode) override = 0;
  virtual IPCCommandResult Close(u32 command_address, bool force) override = 0;
  virtual IPCCommandResult IOCtlV(u32 command_address) override = 0;

  virtual void DoState(PointerWrap& p) override = 0;

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
    CtrlMessage() = default;
    CtrlMessage(const SIOCtlVBuffer& cmd_buffer);

    u8 request_type = 0;
    u8 request = 0;
    u16 value = 0;
    u16 index = 0;
    u16 length = 0;
    u32 payload_addr = 0;
    u32 address = 0;
  };

  class CtrlBuffer
  {
  public:
    CtrlBuffer() = default;
    CtrlBuffer(const SIOCtlVBuffer& cmd_buffer, u32 command_address);

    void FillBuffer(const u8* src, size_t size) const;
    void SetRetVal(const u32 retval) const;
    bool IsValid() const { return m_cmd_address != 0; }
    void Invalidate() { m_cmd_address = m_payload_addr = 0; }
    u8 m_endpoint = 0;
    u16 m_length = 0;
    u32 m_payload_addr = 0;
    u32 m_cmd_address = 0;
  };
};
