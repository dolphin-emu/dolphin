// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_bt_base.h"

class PointerWrap;

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
  void DoState(PointerWrap& p) override;
};
