// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>

#include "Common/BitFieldView.h"
#include "Common/CommonTypes.h"

#include "Core/HW/GCMemcard/GCMemcard.h"

class PointerWrap;

namespace MMIO
{
class Mapping;
}

namespace ExpansionInterface
{
class IEXIDevice;
enum class EXIDeviceType : int;

class CEXIChannel
{
public:
  explicit CEXIChannel(u32 channel_id, const Memcard::HeaderData& memcard_header_data);
  ~CEXIChannel();

  // get device
  IEXIDevice* GetDevice(const u32 chip_select);

  void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

  void SendTransferComplete();

  void AddDevice(EXIDeviceType device_type, int device_num);
  void AddDevice(std::unique_ptr<IEXIDevice> device, int device_num,
                 bool notify_presence_changed = true);

  // Remove all devices
  void RemoveDevices();

  bool IsCausingInterrupt();
  void DoState(PointerWrap& p);
  void PauseAndLock(bool do_lock, bool resume_on_unlock);

  // This should only be used to transition interrupts from SP1 to Channel 2
  void SetEXIINT(bool exiint);

private:
  enum
  {
    EXI_STATUS = 0x00,
    EXI_DMA_ADDRESS = 0x04,
    EXI_DMA_LENGTH = 0x08,
    EXI_DMA_CONTROL = 0x0C,
    EXI_IMM_DATA = 0x10
  };

  // EXI Status Register - "Channel Parameter Register"
  struct EXIStatusRegister
  {
    u32 Hex = 0;

    BFVIEW(bool, 1, 0, EXIINTMASK)
    BFVIEW(bool, 1, 1, EXIINT)
    BFVIEW(bool, 1, 2, TCINTMASK)
    BFVIEW(bool, 1, 3, TCINT)
    BFVIEW(u32, 3, 4, CLK)
    BFVIEW(bool, 1, 7, CS0)
    BFVIEW(bool, 1, 8, CS1)          // Channel 0 only
    BFVIEW(bool, 1, 9, CS2)          // Channel 0 only
    BFVIEW(bool, 1, 10, EXTINTMASK)  // Channel 0, 1 only
    BFVIEW(bool, 1, 11, EXTINT)      // Channel 0, 1 only
    BFVIEW(bool, 1, 12, EXT)         // Channel 0, 1 only | 1: External EXI device present
    BFVIEW(bool, 1, 13, ROMDIS)      // Channel 0 only    | ROM Disable
    BFVIEW(u32, 3, 7, CHIP_SELECT)   // CS0, CS1, and CS2 merged for convenience.

    EXIStatusRegister() = default;
    explicit EXIStatusRegister(u32 hex) : Hex{hex} {}
  };

  // EXI Control Register
  struct EXIControlRegister
  {
    u32 Hex = 0;

    BFVIEW(bool, 1, 0, TSTART)
    BFVIEW(bool, 1, 1, DMA)
    BFVIEW(u32, 2, 2, RW)
    BFVIEW(u32, 2, 4, TLEN)
  };

  // STATE_TO_SAVE
  EXIStatusRegister m_status;
  u32 m_dma_memory_address = 0;
  u32 m_dma_length = 0;
  EXIControlRegister m_control;
  u32 m_imm_data = 0;

  // Since channels operate a bit differently from each other
  u32 m_channel_id;

  // This data is needed in order to reinitialize a GCI folder memory card when switching between
  // GCI folder and other devices in the memory card slot or after loading a savestate. Even though
  // this data is only vaguely related to the EXI_Channel, this seems to be the best place to store
  // it, as this class creates the CEXIMemoryCard instances.
  Memcard::HeaderData m_memcard_header_data;

  // Devices
  enum
  {
    NUM_DEVICES = 3
  };

  std::array<std::unique_ptr<IEXIDevice>, NUM_DEVICES> m_devices;
};
}  // namespace ExpansionInterface
