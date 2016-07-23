// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <windows.h>

#include "Core/HW/WiimoteEmu/WiimoteHid.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{
// Different methods to send data Wiimote on Windows depending on OS and Bluetooth Stack
enum WinWriteMethod
{
  WWM_WRITE_FILE_LARGEST_REPORT_SIZE,
  WWM_WRITE_FILE_ACTUAL_REPORT_SIZE,
  WWM_SET_OUTPUT_REPORT
};

class WiimoteWindows final : public Wiimote
{
public:
  WiimoteWindows(const std::basic_string<TCHAR>& path, WinWriteMethod initial_write_method);
  ~WiimoteWindows() override;

protected:
  bool ConnectInternal() override;
  void DisconnectInternal() override;
  bool IsConnected() const override;
  void IOWakeup() override;
  int IORead(u8* buf) override;
  int IOWrite(u8 const* buf, size_t len) override;

private:
  std::basic_string<TCHAR> m_devicepath;  // Unique Wiimote reference
  HANDLE m_dev_handle;                    // HID handle
  OVERLAPPED m_hid_overlap_read;          // Overlap handles
  OVERLAPPED m_hid_overlap_write;
  WinWriteMethod m_write_method;  // Type of Write Method to use
};

class WiimoteScannerWindows final : public WiimoteScannerBackend
{
public:
  WiimoteScannerWindows();
  ~WiimoteScannerWindows();
  bool IsReady() const override;
  void FindWiimotes(std::vector<Wiimote*>&, Wiimote*&) override;
  void Update() override;

private:
  void CheckDeviceType(std::basic_string<TCHAR>& devicepath, WinWriteMethod& write_method,
                       bool& real_wiimote, bool& is_bb);
};
}
