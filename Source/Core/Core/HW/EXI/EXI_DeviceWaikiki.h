// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/EXI/EXI_Device.h"

namespace ExpansionInterface
{

class CEXIWaikiki : public IEXIDevice
{
public:
  CEXIWaikiki(Core::System& system);
  ~CEXIWaikiki() override;
  void SetCS(int cs) override;
  bool IsPresent() const override;
  bool IsInterruptSet() override;
  void DoState(PointerWrap& p) override;
  void TransferByte(u8& byte) override;

private:
  enum class Command : u8
  {
    // Surprisingly, there don't seem to be any commands to flush the FIFOs or to reset the adapter.
    GET_ID = 0x00,
    GET_STATUS = 0x80,
    IRQ_ENABLE = 0xC0,
    IRQ_DISABLE = 0xC1,
    IRQ_CLEAR = 0xC2,
    READ = 0x88,
    WRITE = 0xF0,
    PROBE_BARNACLE = 0x20,
  };

  u32 m_exi_pos = 0;
  Command m_exi_cmd = Command(0);
  bool m_irq_enabled = false;
  bool m_irq_cleared = true;
  u32 m_response = 0;

  template<u32 SIZE>
  class RingBuffer
  {
  public:
    RingBuffer();
    u32 GetUsableSize() const { return SIZE - 1; }
    u32 GetFillLevel() const;
    void Write(u8 value);
    u8 Read();

  private:
    static constexpr u32 MASK = SIZE - 1;
    u32 m_rp = 0;
    u32 m_wp = 0;
    std::array<u8, SIZE> m_buffer;
  };

  RingBuffer<0x900> m_exi_to_usb;
  RingBuffer<0x440> m_usb_to_exi;
};

}  // namespace ExpansionInterface
