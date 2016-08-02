// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/GCAdapter.h"
#include "Common/CommonTypes.h"
#include "InputCommon/GCPadStatus.h"

namespace GCAdapter
{
void Init()
{
}
void ResetRumble()
{
}
void Shutdown()
{
}
void SetAdapterCallback(std::function<void(void)> func)
{
}
void StartScanThread()
{
}
void StopScanThread()
{
}
GCPadStatus Input(int chan)
{
  return {};
}
void Output(int chan, u8 rumble_command)
{
}
bool IsDetected()
{
  return false;
}
bool IsDriverDetected()
{
  return false;
}
bool DeviceConnected(int chan)
{
  return false;
}
bool UseAdapter()
{
  return false;
}

}  // end of namespace GCAdapter
