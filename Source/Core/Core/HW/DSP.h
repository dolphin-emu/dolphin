// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/BitFieldView.h"
#include "Common/CommonTypes.h"

class PointerWrap;
class DSPEmulator;
namespace MMIO
{
class Mapping;
}

namespace DSP
{
class DSPState
{
public:
  DSPState();
  DSPState(const DSPState&) = delete;
  DSPState(DSPState&&) = delete;
  DSPState& operator=(const DSPState&) = delete;
  DSPState& operator=(DSPState&&) = delete;
  ~DSPState();

  struct Data;
  Data& GetData() { return *m_data; }

private:
  std::unique_ptr<Data> m_data;
};

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

// DSPControl
constexpr u16 DSP_CONTROL_MASK = 0x0C07;
struct DSPControl
{
  u16 Hex;

  // DSP Control
  BFVIEW(bool, 1, 0, DSPReset)  // Write 1 to reset and waits for 0
  BFVIEW(bool, 1, 1, DSPAssertInt)
  BFVIEW(bool, 1, 2, DSPHalt)
  // Interrupt for DMA to the AI/speakers
  BFVIEW(bool, 1, 3, AID)
  BFVIEW(bool, 1, 4, AID_mask)
  // ARAM DMA interrupt
  BFVIEW(bool, 1, 5, ARAM)
  BFVIEW(bool, 1, 6, ARAM_mask)
  // DSP DMA interrupt
  BFVIEW(bool, 1, 7, DSP)
  BFVIEW(bool, 1, 8, DSP_mask)
  // Other ???
  BFVIEW(bool, 1, 9, DMAState)      // DSPGetDMAStatus() uses this flag. __ARWaitForDMA()
                                    // uses it too... maybe it's just general DMA flag
  BFVIEW(bool, 1, 10, DSPInitCode)  // Indicator that the DSP was initialized?
  BFVIEW(bool, 1, 11, DSPInit)      // DSPInit() writes to this flag
  BFVIEW(u16, 4, 12, pad)

  DSPControl(u16 hex = 0) : Hex(hex) {}
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
