// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// The Waikiki is an EXI-USB adapter similar to a USB Gecko but uses a byte-oriented protocol.
//
// https://i.ebayimg.com/images/g/pHsAAOSwvaxnT7Bn/s-l1600.webp
// https://i.ebayimg.com/images/g/husAAOSwn3RnT7Bn/s-l1600.webp
//
// There are two LEDs:
// - slow pulsing green indicates USB power
// - red indicates:
//   - EXI power but no USB power
//   - USB-to-EXI FIFO is full
//
// The SaveMii dongle is based on the Waikiki.

#include "Core/HW/EXI/EXI_DeviceWaikiki.h"

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"

namespace ExpansionInterface
{

CEXIWaikiki::CEXIWaikiki(Core::System& system) : IEXIDevice(system)
{
}

CEXIWaikiki::~CEXIWaikiki()
{
}

void CEXIWaikiki::SetCS(int cs)
{
  if (cs)
    m_exi_pos = 0;
}

bool CEXIWaikiki::IsPresent() const
{
  return true;
}

bool CEXIWaikiki::IsInterruptSet()
{
  // If the USB-to-EXI FIFO is emptied while the IRQ is disabled,
  // enabling it will not trigger the IRQ until more data is written.
  bool irq = m_irq_enabled && m_irq_cleared && m_usb_to_exi.GetFillLevel() != 0;
  if (irq)
    m_irq_cleared = false;
  return irq;
}

template <u32 SIZE>
CEXIWaikiki::RingBuffer<SIZE>::RingBuffer()
{
  m_buffer.fill(0xFF);
}

template <u32 SIZE>
u32 CEXIWaikiki::RingBuffer<SIZE>::GetFillLevel() const
{
  return (m_wp - m_rp) % SIZE;
}

template <u32 SIZE>
void CEXIWaikiki::RingBuffer<SIZE>::Write(u8 value)
{
  if (GetFillLevel() + 1 < SIZE)
  {
    m_wp = (m_wp + 1) % SIZE;
    m_buffer[m_wp] = value;
  }
  else
  {
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "Waikiki: write overflow");
  }
}

template <u32 SIZE>
u8 CEXIWaikiki::RingBuffer<SIZE>::Read()
{
  if (GetFillLevel() > 0)
  {
    m_rp = (m_rp + 1) % SIZE;
    return m_buffer[m_rp];
  }
  else
  {
    // Theory:
    // The USB-to-EXI data path has two chained FIFOs, 0x40 bytes followed by 0x400 bytes.
    // Read underflow will repeat the oldest byte in the larger FIFO.
    // For example, sending the string ("1" * 0x40) + "2" + ("3" * 0x3FF)
    // over USB to a Waikiki with empty FIFOs
    // a) will not block and
    // b) will result in "2" being repeated if you read from EXI beyond the valid data.
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "Waikiki: read underflow");
    return m_buffer[(m_rp - 0x40) % SIZE];
  }
}

void CEXIWaikiki::TransferByte(u8& byte)
{
  u8 input = byte;
  u8& output = byte;
  output = 0;

  if (m_exi_pos == 0)
  {
    m_exi_cmd = Command(input);
  }
  else if (m_exi_pos == 1)
  {
    // The value of this second byte is ignored.
    // However, without it the IRQ commands don't take effect.
    switch (m_exi_cmd)
    {
    case Command::GET_STATUS:
    {
      // TODO: Test at which command byte the fifo sizes are measured.
      u32 read_me = std::min(0x3FFu, m_usb_to_exi.GetFillLevel());
      u32 write_me = std::min(0x7FFu, m_exi_to_usb.GetUsableSize() - m_exi_to_usb.GetFillLevel());
      m_response = write_me << 16 | read_me;
      INFO_LOG_FMT(EXPANSIONINTERFACE, "Waikiki: get fifo status (write_me: {} read_me: {})",
                   write_me, read_me);
      break;
    }
    case Command::IRQ_ENABLE:
      INFO_LOG_FMT(EXPANSIONINTERFACE, "Waikiki: irq enable");
      m_irq_enabled = true;
      break;
    case Command::IRQ_DISABLE:
      INFO_LOG_FMT(EXPANSIONINTERFACE, "Waikiki: irq disable");
      m_irq_enabled = false;
      m_irq_cleared = true;
      break;
    case Command::IRQ_CLEAR:
      INFO_LOG_FMT(EXPANSIONINTERFACE, "Waikiki: irq clear");
      m_irq_cleared = true;
      break;
    case Command::GET_ID:
      m_response = 0x07020030;
      break;
    case Command::READ:
    case Command::WRITE:
      // Nothing to do.
      break;
    case Command::PROBE_BARNACLE:
      // Only defined to avoid the error in the default case below.
      // RVL_DIAG has a bug where it correctly identifies a Waikiki
      // but then probes for a Barnacle anyway.
      INFO_LOG_FMT(EXPANSIONINTERFACE, "Waikiki: buggy barnacle handling");
      break;
    default:
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "Waikiki: unknown command 0x{:02X}", u8(m_exi_cmd));
      break;
    }
  }
  else
  {
    switch (m_exi_cmd)
    {
    case Command::GET_ID:
    case Command::GET_STATUS:
    {
      const u32 response_pos = (m_exi_pos - 2) & 3;
      output = u8(m_response >> (24 - 8 * response_pos));
      break;
    }
    case Command::READ:
      // TODO: verify whether DMA can read/write more than 8 bytes at a time
      output = m_usb_to_exi.Read();
      INFO_LOG_FMT(EXPANSIONINTERFACE, "Waikiki: read 0x{:02X}", output);
      break;
    case Command::WRITE:
      output = input;
      if (input == '\r')
      {
        // We don't emulate the USB side of the adapter yet.
        // For now just log messages sent from EXI.
        std::string line;
        while (m_exi_to_usb.GetFillLevel() > 0)
          line.push_back(m_exi_to_usb.Read());
        NOTICE_LOG_FMT(EXPANSIONINTERFACE, "Waikiki: write '{}'", line.c_str());
      }
      else
      {
        m_exi_to_usb.Write(input);
      }
      // DEBUG: loopback
      m_usb_to_exi.Write(input);
      break;
    case Command::IRQ_ENABLE:
    case Command::IRQ_DISABLE:
    case Command::IRQ_CLEAR:
    default:
      // Nothing to do.
      break;
    }
  }
  ++m_exi_pos;
}

void CEXIWaikiki::DoState(PointerWrap& p)
{
  p.Do(m_exi_pos);
  p.Do(m_exi_cmd);
  p.Do(m_irq_enabled);
  p.Do(m_irq_cleared);
  p.Do(m_response);
  p.Do(m_exi_to_usb);
  p.Do(m_usb_to_exi);
}

}  // namespace ExpansionInterface
