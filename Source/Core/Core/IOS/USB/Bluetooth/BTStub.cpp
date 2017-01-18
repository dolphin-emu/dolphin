// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/Bluetooth/BTStub.h"

#include "Common/ChunkFile.h"
#include "Common/MsgHandler.h"

namespace Core
{
void DisplayMessage(const std::string& message, int time_in_ms);
}

namespace IOS
{
namespace HLE
{
IOSReturnCode CWII_IPC_HLE_Device_usb_oh1_57e_305_stub::Open(const IOSOpenRequest& request)
{
  PanicAlertT("Bluetooth passthrough mode is enabled, but Dolphin was built without libusb."
              " Passthrough mode cannot be used.");
  return IPC_ENOENT;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305_stub::DoState(PointerWrap& p)
{
  Core::DisplayMessage("The current IPC_HLE_Device_usb is a stub. Aborting load.", 4000);
  p.SetMode(PointerWrap::MODE_VERIFY);
}
}  // namespace HLE
}  // namespace IOS
