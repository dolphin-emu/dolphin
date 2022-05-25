// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitField.h"
#include "Common/BitField3.h"
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
struct UDSPControl
{
  u16 Hex;

  // DSP Control
  BFVIEW_M(Hex, bool, 0, 1, DSPReset);  // Write 1 to reset and waits for 0
  BFVIEW_M(Hex, bool, 1, 1, DSPAssertInt);
  BFVIEW_M(Hex, bool, 2, 1, DSPHalt);
  // Interrupt for DMA to the AI/speakers
  BFVIEW_M(Hex, bool, 3, 1, AID);
  BFVIEW_M(Hex, bool, 4, 1, AID_mask);
  // ARAM DMA interrupt
  BFVIEW_M(Hex, bool, 5, 1, ARAM);
  BFVIEW_M(Hex, bool, 6, 1, ARAM_mask);
  // DSP DMA interrupt
  BFVIEW_M(Hex, bool, 7, 1, DSP);
  BFVIEW_M(Hex, bool, 8, 1, DSP_mask);
  // Other ???
  BFVIEW_M(Hex, bool, 9, 1, DMAState);      // DSPGetDMAStatus() uses this flag. __ARWaitForDMA()
                                            // uses it too...maybe it's just general DMA flag
  BFVIEW_M(Hex, bool, 10, 1, DSPInitCode);  // Indicator that the DSP was initialized?
  BFVIEW_M(Hex, bool, 11, 1, DSPInit);      // DSPInit() writes to this flag
  BFVIEW_M(Hex, u16, 12, 4, pad);

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

}  // namespace DSP
