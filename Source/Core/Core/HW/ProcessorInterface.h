// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;

namespace Core
{
class System;
}
namespace CoreTiming
{
struct EventType;
}
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
  PI_FIFO_RESET = 0x18,  // Used by GXAbortFrame
  PI_RESET_CODE = 0x24,
  PI_FLIPPER_REV = 0x2C,
  PI_FLIPPER_UNK = 0x30  // BS1 writes 0x0245248A to it - prolly some bootstrap thing
};

class ProcessorInterfaceManager
{
public:
  explicit ProcessorInterfaceManager(Core::System& system);
  ProcessorInterfaceManager(const ProcessorInterfaceManager& other) = delete;
  ProcessorInterfaceManager(ProcessorInterfaceManager&& other) = delete;
  ProcessorInterfaceManager& operator=(const ProcessorInterfaceManager& other) = delete;
  ProcessorInterfaceManager& operator=(ProcessorInterfaceManager&& other) = delete;
  ~ProcessorInterfaceManager();

  void Init();
  void DoState(PointerWrap& p);

  void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

  u32 GetMask() const { return m_interrupt_mask; }
  u32 GetCause() const { return m_interrupt_cause; }

  void SetInterrupt(u32 cause_mask, bool set = true);

  // Thread-safe func which sets and clears reset button state automagically
  void ResetButton_Tap();
  void PowerButton_Tap();

  u32 m_interrupt_cause = 0;
  u32 m_interrupt_mask = 0;

  // addresses for CPU fifo accesses
  u32 m_fifo_cpu_base = 0;
  u32 m_fifo_cpu_end = 0;
  u32 m_fifo_cpu_write_pointer = 0;

private:
  // Let the PPC know that an external exception is set/cleared
  void UpdateException();

  void SetResetButton(bool set);

  // ID and callback for scheduling reset button presses/releases
  static void ToggleResetButtonCallback(Core::System& system, u64 userdata, s64 cyclesLate);
  static void IOSNotifyResetButtonCallback(Core::System& system, u64 userdata, s64 cyclesLate);
  static void IOSNotifyPowerButtonCallback(Core::System& system, u64 userdata, s64 cyclesLate);

  CoreTiming::EventType* m_event_type_toggle_reset_button = nullptr;
  CoreTiming::EventType* m_event_type_ios_notify_reset_button = nullptr;
  CoreTiming::EventType* m_event_type_ios_notify_power_button = nullptr;

  u32 m_reset_code = 0;

  Core::System& m_system;
};
}  // namespace ProcessorInterface
