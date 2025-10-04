// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WIN32
#include <windows.h>

#include "Common/SocketContext.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/USBUtils.h"

namespace WiimoteReal
{
class WiimoteScannerWindows;

class WiimoteWindows final : public Wiimote
{
  friend WiimoteScannerWindows;

public:
  WiimoteWindows(std::wstring hid_iface);
  ~WiimoteWindows() override;
  std::string GetId() const override;

protected:
  bool ConnectInternal() override;
  void DisconnectInternal() override;
  bool IsConnected() const override;
  void IOWakeup() override;
  int IORead(u8* buf) override;
  int IOWrite(u8 const* buf, size_t len) override;

private:
  // These return 0 on error. -1 on no data.
  int OverlappedRead(u8* data, DWORD size);
  int OverlappedWrite(const u8* data, DWORD size);

  const std::wstring m_hid_iface;

  HANDLE m_dev_handle{INVALID_HANDLE_VALUE};
  HANDLE m_wakeup_event{INVALID_HANDLE_VALUE};

  OVERLAPPED m_hid_overlap_read{};
  OVERLAPPED m_hid_overlap_write{};

  std::atomic_bool m_is_connected{};
};

class WiimoteScannerWindows final : public WiimoteScannerBackend
{
public:
  WiimoteScannerWindows();
  bool IsReady() const override;

  FindResults FindNewWiimotes() override;
  FindResults FindAttachedWiimotes() override;

  void Update() override;
  void RequestStopSearching() override {}

  static void FindAndAuthenticateWiimotes();
  static void RemoveRememberedWiimotes();

private:
  FindResults FindWiimoteHIDDevices();
};
}  // namespace WiimoteReal

#else
#include "Core/HW/WiimoteReal/IODummy.h"
namespace WiimoteReal
{
using WiimoteScannerWindows = WiimoteScannerDummy;
}
#endif
