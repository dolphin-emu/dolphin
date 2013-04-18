// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _EXIINTERFACE_H
#define _EXIINTERFACE_H

#include "Common.h"
#include "EXI_Channel.h"
#include "Thread.h"
class PointerWrap;

namespace ExpansionInterface
{

void Init();
void Shutdown();
void DoState(PointerWrap &p);
void PauseAndLock(bool doLock, bool unpauseOnUnlock);

void Update();
void UpdateInterrupts();

void ChangeDeviceCallback(u64 userdata, int cyclesLate);
void ChangeDevice(const u8 channel, const TEXIDevices device_type, const u8 device_num);
IEXIDevice* FindDevice(TEXIDevices device_type, int customIndex=-1);

void Read32(u32& _uReturnValue, const u32 _iAddress);
void Write32(const u32 _iValue, const u32 _iAddress);

} // end of namespace ExpansionInterface

#endif
