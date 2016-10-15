// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb.h"

class PointerWrap;
class SysConf;

void BackUpBTInfoSection(SysConf* sysconf);
void RestoreBTInfoSection(SysConf* sysconf);

class CWII_IPC_HLE_Device_usb_oh1_57e_305_base : public CWII_IPC_HLE_Device_usb
{
public:
  CWII_IPC_HLE_Device_usb_oh1_57e_305_base(u32 device_id, const std::string& device_name)
      : CWII_IPC_HLE_Device_usb(device_id, device_name)
  {
  }
  virtual ~CWII_IPC_HLE_Device_usb_oh1_57e_305_base() override = default;

  virtual IPCCommandResult Open(u32 command_address, u32 mode) override = 0;
  virtual IPCCommandResult Close(u32 command_address, bool force) override = 0;
  IPCCommandResult IOCtl(u32 command_address) override;
  virtual IPCCommandResult IOCtlV(u32 command_address) override = 0;

  virtual void DoState(PointerWrap& p) override = 0;
  virtual u32 Update() override = 0;

  virtual void UpdateSyncButtonState(bool is_held) {}
  virtual void TriggerSyncButtonPressedEvent() {}
  virtual void TriggerSyncButtonHeldEvent() {}
protected:
  static constexpr int ACL_PKT_SIZE = 339;
  static constexpr int ACL_PKT_NUM = 10;
  static constexpr int SCO_PKT_SIZE = 64;
  static constexpr int SCO_PKT_NUM = 0;

  enum USBEndpoint
  {
    HCI_CTRL = 0x00,
    HCI_EVENT = 0x81,
    ACL_DATA_IN = 0x82,
    ACL_DATA_OUT = 0x02
  };
};
