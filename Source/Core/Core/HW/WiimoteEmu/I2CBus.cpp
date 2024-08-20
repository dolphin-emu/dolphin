// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/I2CBus.h"

#include <algorithm>

namespace WiimoteEmu
{
void I2CBus::AddSlave(I2CSlave* slave)
{
  m_slaves.emplace_back(slave);
}

void I2CBus::RemoveSlave(I2CSlave* slave)
{
  std::erase(m_slaves, slave);
}

void I2CBus::Reset()
{
  m_slaves.clear();
}

int I2CBus::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  for (auto& slave : m_slaves)
  {
    // A slave responded, we are done.
    if (auto const bytes_read = slave->BusRead(slave_addr, addr, count, data_out))
      return bytes_read;
  }

  return 0;
}

int I2CBus::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  for (auto& slave : m_slaves)
  {
    // A slave responded, we are done.
    if (auto const bytes_written = slave->BusWrite(slave_addr, addr, count, data_in))
      return bytes_written;
  }

  return 0;
}

}  // namespace WiimoteEmu
