// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>

#include "Common/CommonTypes.h"

struct GCPadStatus;

namespace GCAdapter
{
// Arbitrarily chosen value that allows the GC Adapter thread to not block if usb interrupts hang.
// Ideally this should be equal to 0, as they shouldn't hang and we don't trigger unnecessary
// libusb transfers.
static constexpr int TIMEOUT = 5000;
enum ControllerTypes
{
  CONTROLLER_NONE = 0,
  CONTROLLER_WIRED = 1,
  CONTROLLER_WIRELESS = 2
};
void Init();
void ResetRumble();
void Shutdown();
void SetAdapterCallback(std::function<void(void)> func);
void StartScanThread();
void StopScanThread();
GCPadStatus Input(int chan);
void Output(int chan, u8 rumble_command);
bool IsDetected();
bool IsDriverDetected();
bool DeviceConnected(int chan);
bool UseAdapter();

}  // end of namespace GCAdapter
