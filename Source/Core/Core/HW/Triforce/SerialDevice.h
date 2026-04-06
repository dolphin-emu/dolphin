// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <span>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace Triforce
{

// Base class for devices that attach to the SerialA/B ports on the Triforce Baseboard.
class SerialDevice
{
public:
  SerialDevice() = default;
  virtual ~SerialDevice() = default;

  SerialDevice(const SerialDevice&) = delete;
  SerialDevice& operator=(const SerialDevice&) = delete;
  SerialDevice(SerialDevice&&) = delete;
  SerialDevice& operator=(SerialDevice&&) = delete;

  void WriteRxBytes(std::span<const u8> bytes);

  std::size_t GetTxByteCount() const { return m_tx_buffer.size(); }

  // Caller should ensure GetTxByteCount() >= byte.size().
  void TakeTxBytes(std::span<u8> bytes);

  virtual void Update() = 0;

  virtual void DoState(PointerWrap& p);

protected:
  std::span<const u8> GetRxByteSpan() const { return m_rx_buffer; }

  void ConsumeRxBytes(std::size_t count);

  void WriteTxByte(u8 byte) { m_tx_buffer.emplace_back(byte); }
  void WriteTxBytes(std::span<const u8> bytes);

  // Transfer the entirety of other's tx buffer to this tx buffer.
  void PassThroughTxBytes(SerialDevice& other);

private:
  // The stream of bytes from the baseboard to the device.
  // FYI: Current device implementations tend to empty the entire buffer in one go,
  //  so std::vector's O(n) erase-at-front should be a non-issue.
  // The contiguous data of std::vector is convenient for packet parsing.
  std::vector<u8> m_rx_buffer;

  // The stream of bytes from the device to the baseboard.
  // It may be read in chunks so std::vector would be less appropriate here.
  std::deque<u8> m_tx_buffer;
};

}  // namespace Triforce
