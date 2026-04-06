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
  m_rx_buffer.insert(m_rx_buffer.end(), bytes.begin(), bytes.end());
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
  m_tx_buffer.insert(m_tx_buffer.end(), bytes.begin(), bytes.end());
#endif
}

void SerialDevice::PassThroughTxBytes(SerialDevice& other)
{
  // The destination buffer will often be empty or near-empty.
  if (m_tx_buffer.size() < other.m_tx_buffer.size()) [[likely]]
  {
#if defined(__cpp_lib_containers_ranges)
    other.m_tx_buffer.prepend_range(m_tx_buffer);
#else
    other.m_tx_buffer.insert(other.m_tx_buffer.begin(), m_tx_buffer.begin(), m_tx_buffer.end());
#endif
    m_tx_buffer.swap(other.m_tx_buffer);
  }
  else
  {
#if defined(__cpp_lib_containers_ranges)
    m_tx_buffer.append_range(other.m_tx_buffer);
#else
    m_tx_buffer.insert(m_tx_buffer.end(), other.m_tx_buffer.begin(), other.m_tx_buffer.end());
#endif
  }

  other.m_tx_buffer.clear();
}

void SerialDevice::DoState(PointerWrap& p)
{
  p.Do(m_rx_buffer);
  p.Do(m_tx_buffer);
}

}  // namespace Triforce
