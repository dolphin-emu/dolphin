// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceModem.h"

#include <inttypes.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
#include <string>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"
#include "Common/StringUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/CoreTiming.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace ExpansionInterface
{

CEXIModem::CEXIModem(Core::System& system, ModemDeviceType type) : IEXIDevice(system)
{
  switch (type)
  {
  case ModemDeviceType::TAPSERVER:
    m_network_interface = std::make_unique<TAPServerNetworkInterface>(
        this, Config::Get(Config::MAIN_MODEM_TAPSERVER_DESTINATION));
    INFO_LOG_FMT(SP1, "Created tapserver physical network interface.");
    break;
  }

  m_regs[Register::DEVICE_TYPE] = 0x02;
  m_regs[Register::INTERRUPT_MASK] = 0x02;
}

CEXIModem::~CEXIModem()
{
  m_network_interface->RecvStop();
  m_network_interface->Deactivate();
}

bool CEXIModem::IsPresent() const
{
  return true;
}

void CEXIModem::SetCS(u32 cs, bool was_selected, bool is_selected)
{
  if (!was_selected && is_selected)
    m_transfer_descriptor = INVALID_TRANSFER_DESCRIPTOR;
}

bool CEXIModem::IsInterruptSet()
{
  return !!(m_regs[Register::INTERRUPT_MASK] & m_regs[Register::PENDING_INTERRUPT_MASK]);
}

void CEXIModem::ImmWrite(u32 data, u32 size)
{
  if (m_transfer_descriptor == INVALID_TRANSFER_DESCRIPTOR)
  {
    m_transfer_descriptor = data;
    if (m_transfer_descriptor == 0x00008000)
    {  // Reset
      m_network_interface->RecvStop();
      m_network_interface->Deactivate();
      m_transfer_descriptor = INVALID_TRANSFER_DESCRIPTOR;
    }
  }
  else if (!IsWriteTransfer(m_transfer_descriptor))
  {
    ERROR_LOG_FMT(SP1, "Received EXI IMM write {:x} ({} bytes) after read command {:x}", data, size,
                  m_transfer_descriptor);
    m_transfer_descriptor = INVALID_TRANSFER_DESCRIPTOR;
  }
  else if (IsModemTransfer(m_transfer_descriptor))
  {  // Write AT command buffer or packet send buffer
    const u32 be_data = htonl(data);
    HandleWriteModemTransfer(&be_data, size);
  }
  else
  {  // Write device register
    u8 reg_num = static_cast<uint8_t>((m_transfer_descriptor >> 24) & 0x1F);
    bool should_update_interrupts = false;
    for (; size && reg_num < m_regs.size(); size--)
    {
      should_update_interrupts |=
          ((reg_num == Register::INTERRUPT_MASK) || (reg_num == Register::PENDING_INTERRUPT_MASK));
      m_regs[reg_num++] = (data >> 24);
      data <<= 8;
    }
    if (should_update_interrupts)
    {
      m_system.GetExpansionInterface().ScheduleUpdateInterrupts(CoreTiming::FromThread::CPU, 0);
    }
    m_transfer_descriptor = INVALID_TRANSFER_DESCRIPTOR;
  }
}

void CEXIModem::DMAWrite(u32 addr, u32 size)
{
  if (m_transfer_descriptor == INVALID_TRANSFER_DESCRIPTOR)
  {
    ERROR_LOG_FMT(SP1, "Received EXI DMA write {:x} ({} bytes) after read command {:x}", addr, size,
                  m_transfer_descriptor);
  }
  else if (!IsWriteTransfer(m_transfer_descriptor))
  {
    ERROR_LOG_FMT(SP1, "Received EXI DMA write {:x} ({} bytes) after read command {:x}", addr, size,
                  m_transfer_descriptor);
    m_transfer_descriptor = INVALID_TRANSFER_DESCRIPTOR;
  }
  else if (!IsModemTransfer(m_transfer_descriptor))
  {
    ERROR_LOG_FMT(SP1, "Received EXI DMA write {:x} ({} bytes) to registers {:x}", addr, size,
                  m_transfer_descriptor);
    m_transfer_descriptor = INVALID_TRANSFER_DESCRIPTOR;
  }
  else
  {
    auto& memory = m_system.GetMemory();
    HandleWriteModemTransfer(memory.GetPointerForRange(addr, size), size);
  }
}

u32 CEXIModem::ImmRead(u32 size)
{
  if (m_transfer_descriptor == INVALID_TRANSFER_DESCRIPTOR)
  {
    ERROR_LOG_FMT(SP1, "Received EXI IMM read ({} bytes) with no pending transfer", size);
    return 0;
  }
  else if (IsWriteTransfer(m_transfer_descriptor))
  {
    ERROR_LOG_FMT(SP1, "Received EXI IMM read ({} bytes) after write command {:x}", size,
                  m_transfer_descriptor);
    m_transfer_descriptor = INVALID_TRANSFER_DESCRIPTOR;
    return 0;
  }
  else if (IsModemTransfer(m_transfer_descriptor))
  {
    u32 be_data = 0;
    HandleReadModemTransfer(&be_data, size);
    return ntohl(be_data);
  }
  else
  {
    // Read device register
    const u8 reg_num = static_cast<uint8_t>((m_transfer_descriptor >> 24) & 0x1F);
    if (reg_num == 0)
    {
      return 0x02020000;  // Device ID (modem)
    }
    u32 ret = 0;
    for (u8 z = 0; z < size; z++)
    {
      if (reg_num + z >= m_regs.size())
      {
        break;
      }
      ret |= (m_regs[reg_num + z] << ((3 - z) * 8));
    }
    m_transfer_descriptor = INVALID_TRANSFER_DESCRIPTOR;
    return ret;
  }
}

void CEXIModem::DMARead(u32 addr, u32 size)
{
  if (m_transfer_descriptor == INVALID_TRANSFER_DESCRIPTOR)
  {
    ERROR_LOG_FMT(SP1, "Received EXI DMA read {:x} ({} bytes) with no pending transfer", addr,
                  size);
  }
  else if (IsWriteTransfer(m_transfer_descriptor))
  {
    ERROR_LOG_FMT(SP1, "Received EXI DMA read {:x} ({} bytes) after write command {:x}", addr, size,
                  m_transfer_descriptor);
    m_transfer_descriptor = INVALID_TRANSFER_DESCRIPTOR;
  }
  else if (!IsModemTransfer(m_transfer_descriptor))
  {
    ERROR_LOG_FMT(SP1, "Received EXI DMA read {:x} ({} bytes) to registers {:x}", addr, size,
                  m_transfer_descriptor);
    m_transfer_descriptor = INVALID_TRANSFER_DESCRIPTOR;
  }
  else
  {
    auto& memory = m_system.GetMemory();
    HandleReadModemTransfer(memory.GetPointerForRange(addr, size), size);
  }
}

void CEXIModem::HandleReadModemTransfer(void* data, u32 size)
{
  const u16 bytes_requested = GetModemTransferSize(m_transfer_descriptor);
  if (size > bytes_requested)
  {
    ERROR_LOG_FMT(SP1, "More bytes requested ({}) than originally requested for transfer {:x}",
                  size, m_transfer_descriptor);
    size = bytes_requested;
  }
  const u16 bytes_requested_after_read = bytes_requested - size;

  if ((m_transfer_descriptor & 0x0F000000) == 0x03000000)
  {
    // AT command buffer
    const std::size_t bytes_to_copy = std::min<std::size_t>(size, m_at_reply_data.size());
    std::memcpy(data, m_at_reply_data.data(), bytes_to_copy);
    m_at_reply_data = m_at_reply_data.substr(bytes_to_copy);
    m_regs[Register::AT_REPLY_SIZE] = static_cast<u8>(m_at_reply_data.size());
    SetInterruptFlag(Interrupt::AT_REPLY_DATA_AVAILABLE, !m_at_reply_data.empty(), true);
  }
  else if ((m_transfer_descriptor & 0x0F000000) == 0x08000000)
  {
    // Packet receive buffer
    std::lock_guard<std::mutex> g(m_receive_buffer_lock);
    const std::size_t bytes_to_copy = std::min<std::size_t>(size, m_receive_buffer.size());
    std::memcpy(data, m_receive_buffer.data(), bytes_to_copy);
    m_receive_buffer = m_receive_buffer.substr(bytes_to_copy);
    OnReceiveBufferSizeChangedLocked(true);
  }
  else
  {
    ERROR_LOG_FMT(SP1, "Invalid modem read transfer type {:x}", m_transfer_descriptor);
  }

  m_transfer_descriptor =
      (bytes_requested_after_read == 0) ?
          INVALID_TRANSFER_DESCRIPTOR :
          SetModemTransferSize(m_transfer_descriptor, bytes_requested_after_read);
}

void CEXIModem::HandleWriteModemTransfer(const void* data, u32 size)
{
  const u16 bytes_expected = GetModemTransferSize(m_transfer_descriptor);
  if (size > bytes_expected)
  {
    ERROR_LOG_FMT(SP1, "More bytes received ({}) than expected for transfer {:x}", size,
                  m_transfer_descriptor);
    return;
  }
  const u16 bytes_expected_after_write = bytes_expected - size;

  if ((m_transfer_descriptor & 0x0F000000) == 0x03000000)
  {  // AT command buffer
    m_at_command_data.append(reinterpret_cast<const char*>(data), size);
    RunAllPendingATCommands();
    m_regs[Register::AT_COMMAND_SIZE] = static_cast<u8>(m_at_command_data.size());
  }
  else if ((m_transfer_descriptor & 0x0F000000) == 0x08000000)
  {  // Packet send buffer
    m_send_buffer.append(reinterpret_cast<const char*>(data), size);
    // A more accurate implementation would only set this interrupt if the send
    // FIFO has enough space; however, we can clear the send FIFO "instantly"
    // from the emulated program's perspective, so we always tell it the send
    // FIFO is empty.
    SetInterruptFlag(Interrupt::SEND_BUFFER_BELOW_THRESHOLD, true, true);
    m_network_interface->SendAndRemoveAllHDLCFrames(&m_send_buffer);
  }
  else
  {
    ERROR_LOG_FMT(SP1, "Invalid modem write transfer type {:x}", m_transfer_descriptor);
  }

  m_transfer_descriptor =
      (bytes_expected_after_write == 0) ?
          INVALID_TRANSFER_DESCRIPTOR :
          SetModemTransferSize(m_transfer_descriptor, bytes_expected_after_write);
}

void CEXIModem::DoState(PointerWrap& p)
{
  // There isn't really any state to save. The registers depend on the state of
  // the external connection, which Dolphin doesn't have control over. What
  // should happen when the user saves a state during an online session and
  // loads it later? The remote server presumably doesn't support point-in-time
  // snapshots and reloading thereof.
}

u16 CEXIModem::GetTxThreshold() const
{
  return (m_regs[Register::TX_THRESHOLD_HIGH] << 8) | m_regs[Register::TX_THRESHOLD_LOW];
}

u16 CEXIModem::GetRxThreshold() const
{
  return (m_regs[Register::RX_THRESHOLD_HIGH] << 8) | m_regs[Register::RX_THRESHOLD_LOW];
}

void CEXIModem::SetInterruptFlag(u8 what, bool enabled, bool from_cpu)
{
  if (enabled)
  {
    m_regs[Register::PENDING_INTERRUPT_MASK] |= what;
  }
  else
  {
    m_regs[Register::PENDING_INTERRUPT_MASK] &= (~what);
  }
  m_system.GetExpansionInterface().ScheduleUpdateInterrupts(
      from_cpu ? CoreTiming::FromThread::CPU : CoreTiming::FromThread::NON_CPU, 0);
}

void CEXIModem::OnReceiveBufferSizeChangedLocked(bool from_cpu)
{
  // The caller is expected to hold m_receive_buffer_lock when calling this.
  const u16 bytes_available =
      static_cast<u16>(std::min<std::size_t>(m_receive_buffer.size(), 0x200));
  m_regs[Register::BYTES_AVAILABLE_HIGH] = (bytes_available >> 8) & 0xFF;
  m_regs[Register::BYTES_AVAILABLE_LOW] = bytes_available & 0xFF;
  SetInterruptFlag(Interrupt::RECEIVE_BUFFER_ABOVE_THRESHOLD,
                   m_receive_buffer.size() >= GetRxThreshold(), from_cpu);
  // TODO: There is a second interrupt here, which the GameCube modem library
  // expects to be used when large frames are received. However, the correct
  // semantics for this interrupt aren't obvious. Reverse-engineering some games
  // that use the modem adapter makes it look like this interrupt should trigger
  // when there's any data at all in the receive buffer, but implementing the
  // interrupt this way causes them to crash. Further research is needed.
  // SetInterruptFlag(Interrupt::RECEIVE_BUFFER_NOT_EMPTY, !m_receive_buffer.empty(), from_cpu);
}

void CEXIModem::SendComplete()
{
  // See comment in HandleWriteModemTransfer about why this is always true.
  SetInterruptFlag(Interrupt::SEND_BUFFER_BELOW_THRESHOLD, true, true);
}

void CEXIModem::AddToReceiveBuffer(std::string&& data)
{
  std::lock_guard<std::mutex> g(m_receive_buffer_lock);
  if (m_receive_buffer.empty())
  {
    m_receive_buffer = std::move(data);
  }
  else
  {
    m_receive_buffer += data;
  }
  OnReceiveBufferSizeChangedLocked(false);
}

void CEXIModem::AddATReply(const std::string& data)
{
  m_at_reply_data += data;
  m_regs[Register::AT_REPLY_SIZE] = static_cast<u8>(m_at_reply_data.size());
  SetInterruptFlag(Interrupt::AT_REPLY_DATA_AVAILABLE, !m_at_reply_data.empty(), true);
}

void CEXIModem::RunAllPendingATCommands()
{
  for (std::size_t newline_pos = m_at_command_data.find_first_of("\r\n");
       newline_pos != std::string::npos; newline_pos = m_at_command_data.find_first_of("\r\n"))
  {
    std::string command = m_at_command_data.substr(0, newline_pos);
    m_at_command_data = m_at_command_data.substr(newline_pos + 1);

    INFO_LOG_FMT(SP1, "Received AT command: {}", command);

    if (command.substr(0, 3) == "ATZ" || command == "ATH0")
    {
      // Reset (ATZ) or hang up (ATH0)
      m_network_interface->RecvStop();
      m_network_interface->Deactivate();
      AddATReply("OK\r");
    }
    else if (command.substr(0, 3) == "ATD")
    {
      // Dial
      if (m_network_interface->Activate())
      {
        m_network_interface->RecvStart();
        AddATReply("OK\rCONNECT 115200\r");  // Maximum baud rate
      }
      else
      {
        AddATReply("OK\rNO ANSWER\r");
      }
    }
    else
    {
      // PSO sends several other AT commands during modem setup, but in our
      // implementation we don't actually have to do anything in response to
      // them, so we just pretend we did.
      AddATReply("OK\r");
    }
  }
}

}  // namespace ExpansionInterface
