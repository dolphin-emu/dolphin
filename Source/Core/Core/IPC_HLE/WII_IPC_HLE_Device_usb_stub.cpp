// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_stub.h"
#include "Common/ChunkFile.h"
#include "Common/MsgHandler.h"

namespace Core
{
void DisplayMessage(const std::string& message, int time_in_ms);
}

IPCCommandResult CWII_IPC_HLE_Device_usb_stub::Open(u32 command_address, u32 mode)
{
  PanicAlertT("Dolphin was built without libusb. Cannot passthrough USB devices.");
  return GetNoReply();
}

void CWII_IPC_HLE_Device_usb_stub::DoState(PointerWrap& p)
{
  Core::DisplayMessage("The current IPC_HLE_Device_usb is a stub. Aborting load.", 4000);
  p.SetMode(PointerWrap::MODE_VERIFY);
}
