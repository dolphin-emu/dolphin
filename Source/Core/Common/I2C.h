// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common
{
class I2CBusOld;

class I2CSlave
{
  friend I2CBusOld;

protected:
  virtual ~I2CSlave() = default;

  virtual int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) = 0;
  virtual int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) = 0;

  template <typename T>
  static int RawRead(T* reg_data, u8 addr, int count, u8* data_out)
  {
    static_assert(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>);
    static_assert(0x100 == sizeof(T));

    // TODO: addr wraps around after 0xff

    u8* src = reinterpret_cast<u8*>(reg_data) + addr;
    count = std::min(count, int(reinterpret_cast<u8*>(reg_data + 1) - src));

    std::copy_n(src, count, data_out);

    return count;
  }

  template <typename T>
  static int RawWrite(T* reg_data, u8 addr, int count, const u8* data_in)
  {
    static_assert(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>);
    static_assert(0x100 == sizeof(T));

    // TODO: addr wraps around after 0xff

    u8* dst = reinterpret_cast<u8*>(reg_data) + addr;
    count = std::min(count, int(reinterpret_cast<u8*>(reg_data + 1) - dst));

    std::copy_n(data_in, count, dst);

    return count;
  }
};

class I2CBusOld
{
public:
  void AddSlave(I2CSlave* slave);
  void RemoveSlave(I2CSlave* slave);

  void Reset();

  // TODO: change int to u16 or something
  int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out);
  int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in);

private:
  // Pointers are unowned:
  std::vector<I2CSlave*> m_slaves;
};

// An I²C bus implementation accessed via bit-banging.
// A few assumptions and limitations exist:
// - All devices support both writes and reads.
// - Timing is not implemented at all; the clock signal can be changed as fast as needed.
// - Devices are not allowed to stretch the clock signal. (Nintendo's write code does not seem to
// implement clock stretching in any case, though some homebrew does.)
// - All devices use a 1-byte auto-incrementing address which wraps around from 255 to 0.
// - The device address is handled by this I2CBus class, instead of the device itself.
// - The device address is set on writes, and re-used for reads; writing an address and data and
// then switching to reading uses the incremented address. Every write must specify the address.
// - Reading without setting the device address beforehand is disallowed; the I²C specification
// allows such reads but does not specify how they behave (or anything about the behavior of the
// device address).
// - Switching between multiple devices using a restart does not reset the device address; the
// device address is only reset on stopping. This means that a write to one device followed by a
// read from a different device would result in reading from the last used device address (without
// any warning).
// - 10-bit addressing and other reserved addressing modes are not implemented.
class I2CBus
{
public:
  enum class State
  {
    Inactive,
    Activating,
    SetI2CAddress,
    WriteDeviceAddress,
    WriteToDevice,
    ReadFromDevice,
  };

  State state;
  u8 bit_counter;
  u8 current_byte;
  std::optional<u8> i2c_address;  // Not shifted; includes the read flag
  std::optional<u8> device_address;

  void Update(const bool old_scl, const bool new_scl, const bool old_sda, const bool new_sda);
  bool GetSCL() const;
  bool GetSDA() const;

private:
  void Start();
  void Stop();
  void SCLRisingEdge(const bool sda);
  void SCLFallingEdge(const bool sda);
  bool WriteExpected() const;
};

}  // namespace Common
