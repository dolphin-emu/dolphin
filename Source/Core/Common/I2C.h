// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <optional>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace Common
{
class I2CSlave
{
public:
  virtual bool Matches(u8 slave_addr) = 0;
  virtual u8 ReadByte(u8 addr) = 0;
  virtual bool WriteByte(u8 addr, u8 value) = 0;

protected:
  ~I2CSlave() = default;

  template<typename T>
  static u8 RawRead(T* reg_data, u8 addr)
  {
    static_assert(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>);
    static_assert(sizeof(T) == 0x100);
    return reinterpret_cast<u8*>(reg_data)[addr];
  }

  template <typename T>
  static void RawWrite(T* reg_data, u8 addr, u8 value)
  {
    static_assert(std::is_standard_layout_v<T> && std::is_trivially_copyable_v<T>);
    static_assert(sizeof(T) == 0x100);
    reinterpret_cast<u8*>(reg_data)[addr] = value;
  }
};

class I2CBusBase
{
public:
  virtual ~I2CBusBase() = default;

  void AddSlave(I2CSlave* slave);
  void RemoveSlave(I2CSlave* slave);

  void Reset();

protected:
  // Returns nullptr if there is no match
  I2CSlave* GetSlave(u8 slave_addr);

private:
  // Pointers are unowned:
  std::vector<I2CSlave*> m_slaves;
};

class I2CBusSimple : public I2CBusBase
{
public:
  // TODO: change int to u16 or something
  int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out);
  int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in);
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
class I2CBus : public I2CBusBase
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

  void DoState(PointerWrap& p);

private:
  void Start();
  void Stop();
  void SCLRisingEdge(const bool sda);
  void SCLFallingEdge(const bool sda);
  bool WriteExpected() const;
};

}  // namespace Common
