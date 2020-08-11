// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SystemTimers.h"

class PointerWrap;

namespace CoreTiming
{
enum class FromThread;
}
namespace MMIO
{
class Mapping;
}

namespace ExpansionInterface
{
class CEXIChannel;
class IEXIDevice;
enum TEXIDevices : int;

enum
{
  MAX_MEMORYCARD_SLOTS = 2,
  MAX_EXI_CHANNELS = 3
};

void Init();
void Shutdown();
void DoState(PointerWrap& p);
void PauseAndLock(bool doLock, bool unpauseOnUnlock);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void UpdateInterrupts();
void ScheduleUpdateInterrupts(CoreTiming::FromThread from, int cycles_late);

// Change a device over a period of time.
// This schedules a device change: First the device is 'ejected' cycles_delay_change cycles from
// now, then the new device is 'inserted' another cycles_no_device_visible cycles later.
void ChangeDevice(const u8 channel, const TEXIDevices device_type, const u8 device_num,
                  CoreTiming::FromThread from_thread = CoreTiming::FromThread::NON_CPU,
                  s64 cycles_delay_change = 0,
                  s64 cycles_no_device_visible = SystemTimers::GetTicksPerSecond());

CEXIChannel* GetChannel(u32 index);

IEXIDevice* FindDevice(TEXIDevices device_type, int customIndex = -1);

}  // end of namespace ExpansionInterface
