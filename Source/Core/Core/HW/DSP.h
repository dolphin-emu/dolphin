// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

class PointerWrap;
class DSPEmulator;
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

class DSPManager
{
public:
  explicit DSPManager(Core::System& system);
  DSPManager(const DSPManager&) = delete;
  DSPManager(DSPManager&&) = delete;
  DSPManager& operator=(const DSPManager&) = delete;
  DSPManager& operator=(DSPManager&&) = delete;
  ~DSPManager();

  void Init(bool hle);
  void Reinit(bool hle);
  void Shutdown();

  void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

  DSPEmulator* GetDSPEmulator();

  void DoState(PointerWrap& p);

  // TODO: Maybe rethink this? The timing is unpredictable.
  void GenerateDSPInterruptFromDSPEmu(DSPInterruptType type, int cycles_into_future = 0);

  // Audio/DSP Helper
  u8 ReadARAM(u32 address) const;
  void WriteARAM(u8 value, u32 address);

  // Debugger Helper
  u8* GetARAMPtr() const;

  void UpdateAudioDMA();
  void UpdateDSPSlice(int cycles);

private:
  void GenerateDSPInterrupt(u64 DSPIntType, s64 cyclesLate);
  static void GlobalGenerateDSPInterrupt(Core::System& system, u64 DSPIntType, s64 cyclesLate);
  void CompleteARAM(u64 userdata, s64 cyclesLate);
  static void GlobalCompleteARAM(Core::System& system, u64 userdata, s64 cyclesLate);
  void UpdateInterrupts();
  void Do_ARAM_DMA();

  // UARAMCount
  union UARAMCount
  {
    u32 Hex = 0;
    struct
    {
      u32 count : 31;
      u32 dir : 1;  // 0: MRAM -> ARAM 1: ARAM -> MRAM
    };
  };

  // Blocks are 32 bytes.
  union UAudioDMAControl
  {
    u16 Hex = 0;
    struct
    {
      u16 NumBlocks : 15;
      u16 Enable : 1;
    };
  };

  // AudioDMA
  struct AudioDMA
  {
    u32 current_source_address = 0;
    u16 remaining_blocks_count = 0;
    u32 SourceAddress = 0;
    UAudioDMAControl AudioDMAControl;
  };

  // ARAM_DMA
  struct ARAM_DMA
  {
    u32 MMAddr = 0;
    u32 ARAddr = 0;
    UARAMCount Cnt;
  };

  // So we may abstract GC/Wii differences a little
  struct ARAMInfo
  {
    bool wii_mode = false;  // Wii EXRAM is managed in Memory:: so we need to skip statesaving, etc
    u32 size = ARAM_SIZE;
    u32 mask = ARAM_MASK;
    u8* ptr = nullptr;  // aka audio ram, auxiliary ram, MEM2, EXRAM, etc...
  };

  union ARAM_Info
  {
    u16 Hex = 0;
    struct
    {
      u16 size : 6;
      u16 unk : 1;
      u16 : 9;
    };
  };

  ARAMInfo m_aram;
  AudioDMA m_audio_dma;
  ARAM_DMA m_aram_dma;
  UDSPControl m_dsp_control;
  ARAM_Info m_aram_info;
  // Contains bitfields for some stuff we don't care about (and nothing ever reads):
  //  CAS latency/burst length/addressing mode/write mode
  // We care about the LSB tho. It indicates that the ARAM controller has finished initializing
  u16 m_aram_mode = 0;
  u16 m_aram_refresh = 0;
  int m_dsp_slice = 0;

  std::unique_ptr<DSPEmulator> m_dsp_emulator;

  bool m_is_lle = false;

  CoreTiming::EventType* m_event_type_generate_dsp_interrupt = nullptr;
  CoreTiming::EventType* m_event_type_complete_aram = nullptr;

  Core::System& m_system;
};
}  // namespace DSP
