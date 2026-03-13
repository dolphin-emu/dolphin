// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Triforce/SerialDevice.h"

#include <algorithm>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"

namespace Triforce
{

void SerialDevice::WriteRxBytes(std::span<const u8> bytes)
{
#if defined(__cpp_lib_containers_ranges)
  m_rx_buffer.append_range(bytes);
#else
  const auto prev_size = m_rx_buffer.size();
  m_rx_buffer.resize(prev_size + bytes.size());
  std::ranges::copy(bytes, m_rx_buffer.begin() + prev_size);
#endif
}

void SerialDevice::TakeTxBytes(std::span<u8> bytes)
{
  DEBUG_ASSERT(m_tx_buffer.size() >= bytes.size());

  std::copy_n(m_tx_buffer.begin(), bytes.size(), bytes.data());
  m_tx_buffer.erase(m_tx_buffer.begin(), m_tx_buffer.begin() + bytes.size());
}

void SerialDevice::ConsumeRxBytes(std::size_t count)
{
  DEBUG_ASSERT(m_rx_buffer.size() >= count);

  m_rx_buffer.erase(m_rx_buffer.begin(), m_rx_buffer.begin() + count);
}

void SerialDevice::WriteTxBytes(std::span<const u8> bytes)
{
#if defined(__cpp_lib_containers_ranges)
  m_tx_buffer.append_range(bytes);
#else
  const auto prev_size = m_tx_buffer.size();
  m_tx_buffer.resize(prev_size + bytes.size());
  std::ranges::copy(bytes, m_tx_buffer.begin() + prev_size);
#endif
}

void SerialDevice::DoState(PointerWrap& p)
{
  p.Do(m_rx_buffer);
  p.Do(m_tx_buffer);
}

}  // namespace Triforce
