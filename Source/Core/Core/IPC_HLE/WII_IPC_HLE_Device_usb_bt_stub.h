// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_base.h"

class CWII_IPC_HLE_Device_usb_oh1_57e_305_stub final
    : public CWII_IPC_HLE_Device_usb_oh1_57e_305_base
{
public:
  CWII_IPC_HLE_Device_usb_oh1_57e_305_stub(u32 device_id, const std::string& device_name)
      : CWII_IPC_HLE_Device_usb_oh1_57e_305_base(device_id, device_name)
  {
  }
  ~CWII_IPC_HLE_Device_usb_oh1_57e_305_stub() override {}
  IPCCommandResult Open(u32 command_address, u32 mode) override;
  IPCCommandResult Close(u32 command_address, bool force) override { return GetNoReply(); }
  IPCCommandResult IOCtl(u32 command_address) override { return GetDefaultReply(); }
  IPCCommandResult IOCtlV(u32 command_address) override { return GetNoReply(); }
  void DoState(PointerWrap& p) override;
  u32 Update() override { return 0; }
};
