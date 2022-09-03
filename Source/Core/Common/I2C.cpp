// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/I2C.h"

#include <algorithm>
#include <bitset>
#include <string_view>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/EnumFormatter.h"
#include "Common/Logging/Log.h"
#include "Core/Debugger/Debugger_SymbolMap.h"

namespace IOS
{
struct AVEState;
extern AVEState ave_state;
extern std::bitset<0x100> ave_ever_logged;  // For logging only; not saved

std::string_view GetAVERegisterName(u8 address);
}  // namespace IOS

namespace Common
{
void I2CBusOld::AddSlave(I2CSlave* slave)
{
  m_slaves.emplace_back(slave);
}

void I2CBusOld::RemoveSlave(I2CSlave* slave)
{
  m_slaves.erase(std::remove(m_slaves.begin(), m_slaves.end(), slave), m_slaves.end());
}

void I2CBusOld::Reset()
{
  m_slaves.clear();
}

int I2CBusOld::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  for (auto& slave : m_slaves)
  {
    auto const bytes_read = slave->BusRead(slave_addr, addr, count, data_out);

    // A slave responded, we are done.
    if (bytes_read)
      return bytes_read;
  }

  return 0;
}

int I2CBusOld::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  for (auto& slave : m_slaves)
  {
    auto const bytes_written = slave->BusWrite(slave_addr, addr, count, data_in);

    // A slave responded, we are done.
    if (bytes_written)
      return bytes_written;
  }

  return 0;
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
  case State::WriteDeviceAddress:
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
  i2c_address.reset();
  // Note: don't reset device_address, as it's re-used for reads
}

void I2CBus::Stop()
{
  INFO_LOG_FMT(WII_IPC, "AVE: Stop I2C");
  state = State::Inactive;
  bit_counter = 0;
  current_byte = 0;
  i2c_address.reset();
  device_address.reset();
}

bool I2CBus::WriteExpected() const
{
  // If we don't have an I²C address, it needs to be written (even if the address that is later
  // written is a read).
  // Otherwise, check the least significant bit; it being *clear* indicates a write.
  const bool is_write = !i2c_address.has_value() || ((i2c_address.value() & 1) == 0);
  // The device that is otherwise recieving instead transmits an acknowledge bit after each byte.
  const bool acknowledge_expected = (bit_counter == 8);

  return is_write ^ acknowledge_expected;
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
  // INFO_LOG_FMT(WII_IPC, "AVE: {} {} rising edge: {} (write expected: {})", bit_counter, state,
  //              sda, WriteExpected());
  // SCL rising edge indicates data clocking. For reads, we set up data at this point.
  // For writes, we instead process it on the falling edge, to better distinguish
  // the start/stop condition.
  if (state == State::ReadFromDevice && bit_counter == 0)
  {
    // Start of a read.
    ASSERT(device_address.has_value());  // Implied by the state transition in falling edge
    current_byte = reinterpret_cast<u8*>(&IOS::ave_state)[device_address.value()];
    INFO_LOG_FMT(WII_IPC, "AVE: Read from {:02x} ({}) -> {:02x}", device_address.value(),
                 IOS::GetAVERegisterName(device_address.value()), current_byte);
  }
  // Dolphin_Debugger::PrintCallstack(Common::Log::LogType::WII_IPC, Common::Log::LogLevel::LINFO);
}

void I2CBus::SCLFallingEdge(const bool sda)
{
  // INFO_LOG_FMT(WII_IPC, "AVE: {} {} falling edge: {} (write expected: {})", bit_counter, state,
  //              sda, WriteExpected());
  // SCL falling edge is used to advance bit_counter/change states and process writes.
  if (state == State::SetI2CAddress || state == State::WriteDeviceAddress ||
      state == State::WriteToDevice)
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
          if ((current_byte >> 1) != 0x70)
          {
            state = State::Inactive;  // NACK
            WARN_LOG_FMT(WII_IPC, "AVE: Unknown I2C address {:02x}", current_byte);
          }
          else
          {
            INFO_LOG_FMT(WII_IPC, "AVE: I2C address is {:02x}", current_byte);
          }
        }
        else if (state == State::WriteDeviceAddress)
        {
          device_address = current_byte;
          INFO_LOG_FMT(WII_IPC, "AVE: Device address is {:02x}", current_byte);
        }
        else
        {
          // Actual write
          ASSERT(state == State::WriteToDevice);
          ASSERT(device_address.has_value());  // implied by state transition
          const u8 old_ave_value = reinterpret_cast<u8*>(&IOS::ave_state)[device_address.value()];
          reinterpret_cast<u8*>(&IOS::ave_state)[device_address.value()] = current_byte;
          if (!IOS::ave_ever_logged[device_address.value()] || old_ave_value != current_byte)
          {
            INFO_LOG_FMT(WII_IPC, "AVE: Wrote {:02x} to {:02x} ({})", current_byte,
                         device_address.value(), IOS::GetAVERegisterName(device_address.value()));
            IOS::ave_ever_logged[device_address.value()] = true;
          }
          else
          {
            DEBUG_LOG_FMT(WII_IPC, "AVE: Wrote {:02x} to {:02x} ({})", current_byte,
                          device_address.value(), IOS::GetAVERegisterName(device_address.value()));
          }
          device_address = device_address.value() + 1;
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
      switch (state)
      {
      case State::SetI2CAddress:
        i2c_address = current_byte;
        // Note: i2c_address is known to correspond to a valid device
        if ((current_byte & 1) == 0)
        {
          state = State::WriteDeviceAddress;
          device_address.reset();
        }
        else
        {
          if (device_address.has_value())
          {
            state = State::ReadFromDevice;
          }
          else
          {
            state = State::Inactive;  // NACK - required for 8-bit internal addresses
            ERROR_LOG_FMT(WII_IPC, "AVE: Attempted to read device without having a read address!");
          }
        }
        break;
      case State::WriteDeviceAddress:
        state = State::WriteToDevice;
        break;
      }
    }
    else
    {
      bit_counter++;
    }
  }
  // Dolphin_Debugger::PrintCallstack(Common::Log::LogType::WII_IPC, Common::Log::LogLevel::LINFO);
}

};  // namespace Common

template <>
struct fmt::formatter<Common::I2CBus::State> : EnumFormatter<Common::I2CBus::State::ReadFromDevice>
{
  static constexpr array_type names = {"Inactive",        "Activating",
                                       "Set I2C Address", "Write Device Address",
                                       "Write To Device", "Read From Device"};
  constexpr formatter() : EnumFormatter(names) {}
};
