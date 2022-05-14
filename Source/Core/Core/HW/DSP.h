// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitField.h"
#include "Common/BitField2.h"
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
constexpr u16 DSP_CONTROL_MASK = 0x0C07;
struct UDSPControl : BitField2<u16>
{
  // DSP Control
  FIELD(bool, 0, 1, DSPReset);  // Write 1 to reset and waits for 0
  FIELD(bool, 1, 1, DSPAssertInt);
  FIELD(bool, 2, 1, DSPHalt);
  // Interrupt for DMA to the AI/speakers
  FIELD(bool, 3, 1, AID);
  FIELD(bool, 4, 1, AID_mask);
  // ARAM DMA interrupt
  FIELD(bool, 5, 1, ARAM);
  FIELD(bool, 6, 1, ARAM_mask);
  // DSP DMA interrupt
  FIELD(bool, 7, 1, DSP);
  FIELD(bool, 8, 1, DSP_mask);
  // Other ???
  FIELD(bool, 9, 1, DMAState);      // DSPGetDMAStatus() uses this flag. __ARWaitForDMA() uses it
                                    // too...maybe it's just general DMA flag
  FIELD(bool, 10, 1, DSPInitCode);  // Indicator that the DSP was initialized?
  FIELD(bool, 11, 1, DSPInit);      // DSPInit() writes to this flag
  FIELD(u16, 12, 4, pad);

  constexpr UDSPControl(u16 val = 0) : BitField2(val) {}
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

}  // namespace DSP
