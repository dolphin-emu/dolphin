// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SI.h"
#include "Core/HW/SystemTimers.h"

#include "InputCommon/GCAdapter.h"
#include "InputCommon/GCPadStatus.h"

namespace GCAdapter
{
void Init()
{
}

void Shutdown()
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
  return;
}

bool IsDetected()
{
  return false;
}

bool IsDriverDetected()
{
  return true;
}

bool DeviceConnected(int chan)
{
  return false;
}

bool UseAdapter()
{
  return SConfig::GetInstance().m_SIDevice[0] == SIDEVICE_WIIU_ADAPTER ||
         SConfig::GetInstance().m_SIDevice[1] == SIDEVICE_WIIU_ADAPTER ||
         SConfig::GetInstance().m_SIDevice[2] == SIDEVICE_WIIU_ADAPTER ||
         SConfig::GetInstance().m_SIDevice[3] == SIDEVICE_WIIU_ADAPTER;
}

void ResetRumble()
{
}

void SetAdapterCallback(std::function<void(void)> func)
{
}

}  // end of namespace GCAdapter
