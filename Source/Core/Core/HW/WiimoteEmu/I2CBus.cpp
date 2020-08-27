// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  m_slaves.erase(std::remove(m_slaves.begin(), m_slaves.end(), slave), m_slaves.end());
}

void I2CBus::Reset()
{
  m_slaves.clear();
}

int I2CBus::BusRead(u8 slave_addr, u8 addr, int count, u8* data_out)
{
  // INFO_LOG(WIIMOTE, "i2c bus read: 0x%02x @ 0x%02x (%d)", slave_addr, addr, count);
  for (auto& slave : m_slaves)
  {
    auto const bytes_read = slave->BusRead(slave_addr, addr, count, data_out);

    // A slave responded, we are done.
    if (bytes_read)
      return bytes_read;
  }

  return 0;
}

int I2CBus::BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in)
{
  // INFO_LOG(WIIMOTE, "i2c bus write: 0x%02x @ 0x%02x (%d)", slave_addr, addr, count);
  for (auto& slave : m_slaves)
  {
    auto const bytes_written = slave->BusWrite(slave_addr, addr, count, data_in);

    // A slave responded, we are done.
    if (bytes_written)
      return bytes_written;
  }

  return 0;
}

}  // namespace WiimoteEmu
