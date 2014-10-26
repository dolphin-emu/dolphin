// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Thread.h"

#include "Core/HW/EXI_Channel.h"

class PointerWrap;
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

void UpdateInterruptsCallback(u64 userdata, int cyclesLate);
void UpdateInterrupts();

void ChangeDeviceCallback(u64 userdata, int cyclesLate);
void ChangeDevice(const u8 channel, const TEXIDevices device_type, const u8 device_num);

CEXIChannel* GetChannel(u32 index);

IEXIDevice* FindDevice(TEXIDevices device_type, int customIndex=-1);

} // end of namespace ExpansionInterface
