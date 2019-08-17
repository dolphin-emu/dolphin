// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>

#include "Common/CommonTypes.h"

struct GCPadStatus;

namespace GCAdapter
{
enum ControllerTypes
{
  CONTROLLER_NONE = 0,
  CONTROLLER_WIRED = 1,
  CONTROLLER_WIRELESS = 2
};
struct origin_data
{
  u8 stickX;
  u8 stickY;
  u8 substickX;
  u8 substickY;
  u8 triggerL;
  u8 triggerR;
};

void Init();
void ResetRumble();
void Shutdown();
void SetAdapterCallback(std::function<void(void)> func);
void StartScanThread();
void StopScanThread();
GCPadStatus Input(int chan);
void Output(int chan, u8 rumble_command);
bool IsDetected(const char** error_message);
bool DeviceConnected(int chan);
void ResetDeviceType(int chan);
bool UseAdapter();
void GetOrigin(int chan, origin_data* origin);

}  // end of namespace GCAdapter
