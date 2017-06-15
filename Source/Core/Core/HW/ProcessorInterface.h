// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;

namespace MMIO
{
class Mapping;
}

// Holds statuses of things like the write gatherer used for fifos, and interrupts from various
// sources

namespace ProcessorInterface
{
enum InterruptCause
{
  INT_CAUSE_PI = 0x1,           // YAGCD says: GP runtime error
  INT_CAUSE_RSW = 0x2,          // Reset Switch
  INT_CAUSE_DI = 0x4,           // DVD interrupt
  INT_CAUSE_SI = 0x8,           // Serial interface
  INT_CAUSE_EXI = 0x10,         // Expansion interface
  INT_CAUSE_AI = 0x20,          // Audio Interface Streaming
  INT_CAUSE_DSP = 0x40,         // DSP interface
  INT_CAUSE_MEMORY = 0x80,      // Memory interface
  INT_CAUSE_VI = 0x100,         // Video interface
  INT_CAUSE_PE_TOKEN = 0x200,   // GP Token
  INT_CAUSE_PE_FINISH = 0x400,  // GP Finished
  INT_CAUSE_CP = 0x800,         // Command Fifo
  INT_CAUSE_DEBUG = 0x1000,     // Debugger (from devkit)
  INT_CAUSE_HSP = 0x2000,       // High Speed Port (from sdram controller)
  INT_CAUSE_WII_IPC = 0x4000,   // Wii IPC
  INT_CAUSE_RST_BUTTON =
      0x10000  // ResetButtonState (1 = unpressed, 0 = pressed) it's a state, not maskable
};

// Internal hardware addresses
enum
{
  PI_INTERRUPT_CAUSE = 0x00,
  PI_INTERRUPT_MASK = 0x04,
  PI_FIFO_BASE = 0x0C,
  PI_FIFO_END = 0x10,
  PI_FIFO_WPTR = 0x14,
  PI_FIFO_RESET = 0x18,  // ??? - GXAbortFrame writes to it
  PI_RESET_CODE = 0x24,
  PI_FLIPPER_REV = 0x2C,
  PI_FLIPPER_UNK = 0x30  // BS1 writes 0x0245248A to it - prolly some bootstrap thing
};

extern u32 m_InterruptCause;
extern u32 m_InterruptMask;
extern u32 Fifo_CPUBase;
extern u32 Fifo_CPUEnd;
extern u32 Fifo_CPUWritePointer;

void Init();
void DoState(PointerWrap& p);

void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

inline u32 GetMask()
{
  return m_InterruptMask;
}
inline u32 GetCause()
{
  return m_InterruptCause;
}

void SetInterrupt(u32 _causemask, bool _bSet = true);

// Thread-safe func which sets and clears reset button state automagically
void ResetButton_Tap();
void PowerButton_Tap();

}  // namespace ProcessorInterface
