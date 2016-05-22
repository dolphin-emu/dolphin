// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

class PointerWrap;
class ISIDevice;
enum SIDevices : int;
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
void AddDevice(std::unique_ptr<ISIDevice> device);

void ChangeDevice(SIDevices device, int channel);
void ChangeDeviceDeterministic(SIDevices device, int channel);

SIDevices GetDeviceType(int channel);

u32 GetPollXLines();

} // end of namespace SerialInterface
