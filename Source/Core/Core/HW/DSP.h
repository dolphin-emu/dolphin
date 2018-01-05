// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;
class DSPEmulator;
namespace MMIO
{
class Mapping;
}

namespace DSP
{
enum DSPInterruptType
{
  INT_DSP = 0x80,
  INT_ARAM = 0x20,
  INT_AID = 0x08,
};

// aram size and mask
enum
{
  ARAM_SIZE = 0x01000000,  // 16 MB
  ARAM_MASK = 0x00FFFFFF,
};

// UDSPControl
#if defined(_MSC_VER) && _MSC_VER <= 1800
const u16 DSP_CONTROL_MASK = 0x0C07;
#else
constexpr u16 DSP_CONTROL_MASK = 0x0C07;
#endif
union UDSPControl
{
  u16 Hex;
  struct
  {
    // DSP Control
    u16 DSPReset : 1;  // Write 1 to reset and waits for 0
    u16 DSPAssertInt : 1;
    u16 DSPHalt : 1;
    // Interrupt for DMA to the AI/speakers
    u16 AID : 1;
    u16 AID_mask : 1;
    // ARAM DMA interrupt
    u16 ARAM : 1;
    u16 ARAM_mask : 1;
    // DSP DMA interrupt
    u16 DSP : 1;
    u16 DSP_mask : 1;
    // Other ???
    u16 DMAState : 1;     // DSPGetDMAStatus() uses this flag. __ARWaitForDMA() uses it too...maybe
                          // it's just general DMA flag
    u16 DSPInitCode : 1;  // Indicator that the DSP was initialized?
    u16 DSPInit : 1;      // DSPInit() writes to this flag
    u16 pad : 4;
  };
  UDSPControl(u16 hex = 0) : Hex(hex) {}
};

void Init(bool hle);
void Reinit(bool hle);
void Shutdown();

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

DSPEmulator* GetDSPEmulator();

void DoState(PointerWrap& p);

// TODO: Maybe rethink this? The timing is unpredictable.
void GenerateDSPInterruptFromDSPEmu(DSPInterruptType type, int cycles_into_future = 0);

// Audio/DSP Helper
u8 ReadARAM(u32 address);
void WriteARAM(u8 value, u32 address);

// Debugger Helper
u8* GetARAMPtr();

void UpdateAudioDMA();
void UpdateDSPSlice(int cycles);

}  // end of namespace DSP
