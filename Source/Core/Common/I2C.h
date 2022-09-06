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
  virtual ~I2CSlave() = default;

  // NOTE: slave_addr is 7 bits, i.e. it has been shifted so the read flag is not included.
  virtual bool StartWrite(u8 slave_addr) = 0;
  virtual bool StartRead(u8 slave_addr) = 0;
  virtual void Stop() = 0;
  virtual std::optional<u8> ReadByte() = 0;
  virtual bool WriteByte(u8 value) = 0;
};

class I2CSlaveAutoIncrementing : I2CSlave
{
public:
  I2CSlaveAutoIncrementing(u8 slave_addr) : m_slave_addr(slave_addr) {}
  virtual ~I2CSlaveAutoIncrementing() = default;

  bool StartWrite(u8 slave_addr) override;
  bool StartRead(u8 slave_addr) override;
  void Stop() override;
  std::optional<u8> ReadByte() override;
  bool WriteByte(u8 value) override;

  virtual void DoState(PointerWrap& p);

protected:
  virtual u8 ReadByte(u8 addr) = 0;
  virtual void WriteByte(u8 addr, u8 value) = 0;

private:
  const u8 m_slave_addr;
  bool m_active = false;
  std::optional<u8> m_device_address = std::nullopt;
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
// - 10-bit addressing and other reserved addressing modes are not implemented.
class I2CBus : public I2CBusBase
{
public:
  enum class State
  {
    Inactive,
    Activating,
    SetI2CAddress,
    WriteToDevice,
    ReadFromDevice,
  };

  State state;
  u8 bit_counter;
  u8 current_byte;

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
