// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/Bluetooth/BTStub.h"

#include "Common/ChunkFile.h"
#include "Common/MsgHandler.h"
#include "Core/Core.h"

namespace IOS::HLE::Device
{
IPCCommandResult BluetoothStub::Open(const OpenRequest& request)
{
  PanicAlertT("Bluetooth passthrough mode is enabled, but Dolphin was built without libusb."
              " Passthrough mode cannot be used.");
  return GetDefaultReply(IPC_ENOENT);
}

void BluetoothStub::DoState(PointerWrap& p)
{
  Core::DisplayMessage("The current IPC_HLE_Device_usb is a stub. Aborting load.", 4000);
  p.SetMode(PointerWrap::MODE_VERIFY);
}
}  // namespace IOS::HLE::Device
