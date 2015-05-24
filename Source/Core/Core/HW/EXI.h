// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class CEXIChannel;
class IEXIDevice;
class PointerWrap;
enum TEXIDevices : int;
namespace MMIO { class Mapping; }

enum
{
	MAX_EXI_CHANNELS = 3
};

namespace ExpansionInterface
{

void Init();
void Shutdown();
void DoState(PointerWrap &p);
void PauseAndLock(bool doLock, bool unpauseOnUnlock);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void UpdateInterrupts();
void ScheduleUpdateInterrupts_Threadsafe(int cycles_late);
void ScheduleUpdateInterrupts(int cycles_late);

void ChangeDevice(const u8 channel, const TEXIDevices device_type, const u8 device_num);

CEXIChannel* GetChannel(u32 index);

IEXIDevice* FindDevice(TEXIDevices device_type, int customIndex=-1);

} // end of namespace ExpansionInterface
