// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/I2C.h"

#include <algorithm>
#include <bitset>
#include <string_view>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/EnumFormatter.h"
#include "Common/Logging/Log.h"
#include "Core/Debugger/Debugger_SymbolMap.h"

namespace Common
{
void I2CBusBase::AddSlave(I2CSlave* slave)
{
  m_slaves.emplace_back(slave);
}

void I2CBusBase::RemoveSlave(I2CSlave* slave)
{
  m_slaves.erase(std::remove(m_slaves.begin(), m_slaves.end(), slave), m_slaves.end());
}

void I2CBusBase::Reset()
{
  m_slaves.clear();
}

int I2CBusSimple::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  if (slave != nullptr)
  {
    for (int i = 0; i < count; i++)
    {
      // TODO: Does this make sense? The transmitter can't NACK a read... it's the receiver that
      // does that
      auto byte = slave->ReadByte();
      if (byte.has_value())
        data_out[i] = byte.value();
      else
        return i;
    }
    return count;
  }
  else
  {
    return 0;
  }
}

int I2CBusSimple::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  I2CSlave* slave = GetSlave(slave_addr);
  if (slave != nullptr)
  {
    for (int i = 0; i < count; i++)
    {
      if (!slave->WriteByte(data_in[i]))
        return i;
    }
    return count;
  }
  else
  {
    return 0;
  }
}

bool I2CBus::GetSCL() const
{
  return true;  // passive pullup - no clock stretching
}

bool I2CBus::GetSDA() const
{
  switch (state)
  {
  case State::Inactive:
  case State::Activating:
  default:
    return true;  // passive pullup (or NACK)

  case State::SetI2CAddress:
  case State::WriteToDevice:
    if (bit_counter < 8)
      return true;  // passive pullup
    else
      return false;  // ACK (if we need to NACK, we set the state to inactive)

  case State::ReadFromDevice:
    if (bit_counter < 8)
      return ((current_byte << bit_counter) & 0x80) != 0;
    else
      return true;  // passive pullup, receiver needs to ACK or NACK
  }
}

void I2CBus::Start()
{
  if (state != State::Inactive)
    INFO_LOG_FMT(WII_IPC, "AVE: Re-start I2C");
  else
    INFO_LOG_FMT(WII_IPC, "AVE: Start I2C");

  if (bit_counter != 0)
    WARN_LOG_FMT(WII_IPC, "I2C: Start happened with a nonzero bit counter: {}", bit_counter);

  state = State::Activating;
  bit_counter = 0;
  current_byte = 0;
  // Note: don't reset device_address, as it's re-used for reads
}

void I2CBus::Stop()
{
  INFO_LOG_FMT(WII_IPC, "AVE: Stop I2C");
  for (I2CSlave* slave : m_slaves)
  {
    slave->Stop();
  }

  state = State::Inactive;
  bit_counter = 0;
  current_byte = 0;
}

void I2CBus::Update(const bool old_scl, const bool new_scl, const bool old_sda, const bool new_sda)
{
  if (old_scl != new_scl && old_sda != new_sda)
  {
    ERROR_LOG_FMT(WII_IPC, "Both SCL and SDA changed at the same time: SCL {} -> {} SDA {} -> {}",
                  old_scl, new_scl, old_sda, new_sda);
    return;
  }

  if (old_scl == new_scl && old_sda == new_sda)
    return;  // Nothing changed

  if (old_scl && new_scl)
  {
    // Check for changes to SDA while the clock is high.
    if (old_sda && !new_sda)
    {
      // SDA falling edge (now pulled low) while SCL is high indicates I²C start
      Start();
    }
    else if (!old_sda && new_sda)
    {
      // SDA rising edge (now passive pullup) while SCL is high indicates I²C stop
      Stop();
    }
  }
  else if (state != State::Inactive)
  {
    if (!old_scl && new_scl)
    {
      SCLRisingEdge(new_sda);
    }
    else if (old_scl && !new_scl)
    {
      SCLFallingEdge(new_sda);
    }
  }
}

void I2CBus::SCLRisingEdge(const bool sda)
{
  // INFO_LOG_FMT(WII_IPC, "AVE: {} {} rising edge: {}", bit_counter, state, sda);
  // SCL rising edge indicates data clocking. For reads, we set up data at this point.
  // For writes, we instead process it on the falling edge, to better distinguish
  // the start/stop condition.
  if (state == State::ReadFromDevice && bit_counter == 0)
  {
    // Start of a read.
    std::optional<u8> byte = std::nullopt;
    for (I2CSlave* slave : m_slaves)
    {
      std::optional<u8> byte2 = slave->ReadByte();
      if (byte.has_value() && byte2.has_value())
      {
        WARN_LOG_FMT(WII_IPC, "Multiple slaves responded to read: {:02x} vs {:02x}", *byte, *byte2);
      }
      else if (byte2.has_value())
      {
        byte = byte2;
      }
    }
    if (!byte.has_value())
    {
      WARN_LOG_FMT(WII_IPC, "No slaves responded to I2C read");
      current_byte = 0xff;
    }
    else
    {
      current_byte = byte.value();
    }
    // INFO_LOG_FMT(WII_IPC, "AVE: Read from {:02x} ({}) -> {:02x}", device_address.value(),
    //              IOS::GetAVERegisterName(device_address.value()), current_byte);
  }
  // Dolphin_Debugger::PrintCallstack(Common::Log::LogType::WII_IPC, Common::Log::LogLevel::LINFO);
}

void I2CBus::SCLFallingEdge(const bool sda)
{
  // INFO_LOG_FMT(WII_IPC, "AVE: {} {} falling edge: {}", bit_counter, state, sda);
  // SCL falling edge is used to advance bit_counter/change states and process writes.
  if (state == State::SetI2CAddress || state == State::WriteToDevice)
  {
    if (bit_counter == 8)
    {
      // Acknowledge bit for *reads*.
      if (sda)
      {
        WARN_LOG_FMT(WII_IPC, "Read NACK'd");
        state = State::Inactive;
      }
    }
    else
    {
      current_byte <<= 1;
      if (sda)
        current_byte |= 1;

      if (bit_counter == 7)
      {
        INFO_LOG_FMT(WII_IPC, "AVE: Byte written: {:02x}", current_byte);
        // Write finished.
        if (state == State::SetI2CAddress)
        {
          bool got_ack = false;
          for (I2CSlave* slave : m_slaves)
          {
            if (current_byte & 1)
            {
              if (slave->StartRead(current_byte >> 1))
              {
                if (got_ack)
                {
                  WARN_LOG_FMT(WII_IPC, "Multiple slaves ACKed starting read for I2C addr {:02x}",
                               current_byte);
                }
                got_ack = true;
              }
            }
            else  // (current_byte & 1) == 0
            {
              if (slave->StartWrite(current_byte >> 1))
              {
                if (got_ack)
                {
                  WARN_LOG_FMT(WII_IPC, "Multiple slaves ACKed starting write for I2C addr {:02x}",
                               current_byte);
                }
                got_ack = true;
              }
            }
          }

          if (!got_ack)
          {
            state = State::Inactive;  // NACK
            WARN_LOG_FMT(WII_IPC, "AVE: Unknown I2C address {:02x}", current_byte);
          }
          else
          {
            // State transition handled by bit_counter >= 8, as we still need to handle the ACK
            INFO_LOG_FMT(WII_IPC, "AVE: I2C address is {:02x}", current_byte);
          }
        }
        else
        {
          // Actual write
          ASSERT(state == State::WriteToDevice);
          bool got_ack = false;
          for (I2CSlave* slave : m_slaves)
          {
            if (slave->WriteByte(current_byte))
            {
              if (got_ack)
              {
                WARN_LOG_FMT(WII_IPC, "Multiple slaves responded to write of {:02x}", current_byte);
              }
              got_ack = true;
            }
          }
        }
      }
    }
  }

  if (state == State::Activating)
  {
    // This is triggered after the start condition.
    state = State::SetI2CAddress;
    bit_counter = 0;
  }
  else if (state != State::Inactive)
  {
    if (bit_counter >= 8)
    {
      // Finished a byte and the acknowledge signal.
      bit_counter = 0;
      if (state == State::SetI2CAddress)
      {
        // Note: current_byte is known to correspond to a valid device
        if ((current_byte & 1) == 0)
        {
          state = State::WriteToDevice;
        }
        else
        {
          state = State::ReadFromDevice;
        }
      }
    }
    else
    {
      bit_counter++;
    }
  }
  // Dolphin_Debugger::PrintCallstack(Common::Log::LogType::WII_IPC, Common::Log::LogLevel::LINFO);
}

void I2CBus::DoState(PointerWrap& p)
{
  p.Do(state);
  p.Do(bit_counter);
  p.Do(current_byte);
  // TODO: verify m_devices is the same so that the state is compatible.
  // (We don't take ownership of saving/loading m_devices, though).
}

bool I2CSlaveAutoIncrementing::StartWrite(u8 slave_addr)
{
  if (slave_addr == m_slave_addr)
  {
    INFO_LOG_FMT(WII_IPC, "I2C Device {:02x} write started, previously active: {}", m_slave_addr,
                 m_active);
    m_active = true;
    m_device_address.reset();
    return true;
  }
  else
  {
    return false;
  }
}

bool I2CSlaveAutoIncrementing::StartRead(u8 slave_addr)
{
  if (slave_addr == m_slave_addr)
  {
    INFO_LOG_FMT(WII_IPC, "I2C Device {:02x} read started, previously active: {}", m_slave_addr,
                 m_active);
    if (m_device_address.has_value())
    {
      m_active = true;
      return true;
    }
    else
    {
      WARN_LOG_FMT(WII_IPC,
                   "I2C Device {:02x}: read attempted without having written device address",
                   m_slave_addr);
      m_active = false;
      return false;
    }
  }
  else
  {
    return false;
  }
}

void I2CSlaveAutoIncrementing::Stop()
{
  m_active = false;
  m_device_address.reset();
}

std::optional<u8> I2CSlaveAutoIncrementing::ReadByte()
{
  if (m_active)
  {
    ASSERT(m_device_address.has_value());  // enforced by StartRead
    const u8 cur_addr = m_device_address.value();
    m_device_address = cur_addr + 1;  // wrapping from 255 to 0 is the assumed behavior
    return ReadByte(cur_addr);
  }
  else
  {
    return std::nullopt;
  }
}

bool I2CSlaveAutoIncrementing::WriteByte(u8 value)
{
  if (m_active)
  {
    if (m_device_address.has_value())
    {
      WriteByte(m_device_address.value(), value);
    }
    else
    {
      m_device_address = value;
    }
    return true;
  }
  else
  {
    return false;
  }
}

void I2CSlaveAutoIncrementing::DoState(PointerWrap& p)
{
  u8 slave_addr = m_slave_addr;
  p.Do(slave_addr);
  if (slave_addr != m_slave_addr && p.IsReadMode())
  {
    PanicAlertFmt("State incompatible: Mismatched I2C address: expected {:02x}, was {:02x}. "
                  "Aborting state load.",
                  m_slave_addr, slave_addr);
    p.SetVerifyMode();
  }
  p.Do(m_active);
  p.Do(m_device_address);
}

};  // namespace Common

template <>
struct fmt::formatter<Common::I2CBus::State> : EnumFormatter<Common::I2CBus::State::ReadFromDevice>
{
  static constexpr array_type names = {"Inactive", "Activating", "Set I2C Address",
                                       "Write To Device", "Read From Device"};
  constexpr formatter() : EnumFormatter(names) {}
};
