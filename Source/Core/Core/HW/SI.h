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

// SI Channel Output
union USIChannelOut
{
	u32 Hex;
	struct
	{
		u32 OUTPUT1 : 8;
		u32 OUTPUT0 : 8;
		u32 CMD     : 8;
		u32         : 8;
	};
};

// SI Channel Input High u32
union USIChannelIn_Hi
{
	u32 Hex;
	struct
	{
		u32 INPUT3   : 8;
		u32 INPUT2   : 8;
		u32 INPUT1   : 8;
		u32 INPUT0   : 6;
		u32 ERRLATCH : 1; // 0: no error  1: Error latched. Check SISR.
		u32 ERRSTAT  : 1; // 0: no error  1: error on last transfer
	};
};

// SI Channel Input Low u32
union USIChannelIn_Lo
{
	u32 Hex;
	struct
	{
		u32 INPUT7 : 8;
		u32 INPUT6 : 8;
		u32 INPUT5 : 8;
		u32 INPUT4 : 8;
	};
};

// SI Channel
struct SSIChannel
{
	USIChannelOut   m_Out;
	USIChannelIn_Hi m_InHi;
	USIChannelIn_Lo m_InLo;
	ISIDevice*      m_pDevice;
};

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
