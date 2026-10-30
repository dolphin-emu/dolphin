// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Wii Remotes provided by the wmhcid service, which drives a USB Bluetooth adapter
// directly and exposes each connected remote as a Unix socket in /tmp/wmhci/.
// Frames on the socket are a length byte followed by the raw HID report
// (including the 0xA1/0xA2 header byte).

#if defined(__APPLE__) || defined(__linux__)

#include <string>

#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{
class WiimoteWmhci final : public Wiimote
{
public:
  explicit WiimoteWmhci(std::string socket_path);
  ~WiimoteWmhci() override;
  std::string GetId() const override { return m_socket_path; }

protected:
  bool ConnectInternal() override;
  void DisconnectInternal() override;
  bool IsConnected() const override;
  void IOWakeup() override;
  int IORead(u8* buf) override;
  int IOWrite(const u8* buf, size_t len) override;

private:
  bool ReadExact(u8* buf, size_t len);

  const std::string m_socket_path;
  int m_fd = -1;
  int m_wakeup_pipe[2] = {-1, -1};
};

class WiimoteScannerWmhci final : public WiimoteScannerBackend
{
public:
  FindResults FindAttachedWiimotes() override;
  void Update() override {}
  void RequestStopSearching() override {}
};
}  // namespace WiimoteReal

#else
#include "Core/HW/WiimoteReal/IODummy.h"
namespace WiimoteReal
{
using WiimoteScannerWmhci = WiimoteScannerDummy;
}
#endif
