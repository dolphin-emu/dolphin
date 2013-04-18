// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _SERIALINTERFACE_H
#define _SERIALINTERFACE_H

#include "Common.h"
#include "SI_Device.h"
class PointerWrap;
class ISIDevice;

namespace SerialInterface
{

void Init();
void Shutdown();
void DoState(PointerWrap &p);

void UpdateDevices();

void RemoveDevice(int _iDeviceNumber);
void AddDevice(const SIDevices _device, int _iDeviceNumber);
void AddDevice(ISIDevice* pDevice);

void ChangeDeviceCallback(u64 userdata, int cyclesLate);
void ChangeDevice(SIDevices device, int channel);

void Read32(u32& _uReturnValue, const u32 _iAddress);
void Write32(const u32 _iValue, const u32 _iAddress);

int GetTicksToNextSIPoll();

}; // end of namespace SerialInterface

#endif
