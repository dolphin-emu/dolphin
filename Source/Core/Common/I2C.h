// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common
{
class I2CBus;

class I2CSlave
{
  friend I2CBus;

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

class I2CBus
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

}  // namespace Common
