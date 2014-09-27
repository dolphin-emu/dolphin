// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/SI_Device.h"

class PointerWrap;
class ISIDevice;
namespace MMIO { class Mapping; }

// SI number of channels
enum
{
	MAX_SI_CHANNELS = 0x04
};

namespace SerialInterface
{

void Init();
void Shutdown();
void DoState(PointerWrap &p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

void UpdateDevices();

void RemoveDevice(int _iDeviceNumber);
void AddDevice(const SIDevices _device, int _iDeviceNumber);
void AddDevice(ISIDevice* pDevice);

void ChangeDeviceCallback(u64 userdata, int cyclesLate);
void ChangeDevice(SIDevices device, int channel);

SIDevices GetDeviceType(int channel);

int GetTicksToNextSIPoll();

} // end of namespace SerialInterface
