// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(__linux__) && HAVE_BLUEZ

#include <atomic>

#include "Common/Network.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{
class WiimoteLinux final : public Wiimote
{
public:
  explicit WiimoteLinux(Common::BluetoothAddress bdaddr);
  ~WiimoteLinux() override;

  std::string GetId() const override;

protected:
  bool ConnectInternal() override;
  void DisconnectInternal() override;
  bool IsConnected() const override;
  void IOWakeup() override;
  int IORead(u8* buf) override;
  int IOWrite(u8 const* buf, size_t len) override;

private:
  const Common::BluetoothAddress m_bdaddr;
  const int m_wakeup_fd{-1};  // Used to kick the read thread.
  int m_cmd_sock{-1};         // Command socket
  int m_int_sock{-1};         // Interrupt socket
};

class WiimoteScannerLinux final : public WiimoteScannerBackend
{
public:
  WiimoteScannerLinux();
  ~WiimoteScannerLinux() override;

  bool IsReady() const override;

  // FYI: This backend only supports connecting remotes just found via Bluetooth inquiry.
  FindResults FindNewWiimotes() override;

  void Update() override;
  void RequestStopSearching() override;

private:
  bool Open();
  void Close();

  int m_device_id{-1};
  int m_device_sock{-1};

  // FYI: Atomic because UI calls IsReady.
  std::atomic<bool> m_is_device_open{};
};

}  // namespace WiimoteReal

#else
#include "Core/HW/WiimoteReal/IODummy.h"
namespace WiimoteReal
{
using WiimoteScannerLinux = WiimoteScannerDummy;
}
#endif
