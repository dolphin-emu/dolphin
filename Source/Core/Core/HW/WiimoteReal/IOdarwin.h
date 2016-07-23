// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// Work around an Apple bug: for some reason, IOBluetooth.h errors on
// inclusion in Mavericks, but only in Objective-C++ C++11 mode.  I filed
// this as <rdar://15312520>; in the meantime...
#import <Foundation/Foundation.h>
#undef NS_ENUM_AVAILABLE
#define NS_ENUM_AVAILABLE(...)
// end hack
#import <IOBluetooth/IOBluetooth.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{
class WiimoteDarwin final : public Wiimote
{
public:
  WiimoteDarwin(IOBluetoothDevice* device);
  ~WiimoteDarwin() override;

  // These are not protected/private because ConnectBT needs them.
  void DisconnectInternal() override;
  IOBluetoothDevice* m_btd;
  unsigned char* m_input;
  int m_inputlen;

protected:
  bool ConnectInternal() override;
  bool IsConnected() const override;
  void IOWakeup() override;
  int IORead(u8* buf) override;
  int IOWrite(u8 const* buf, size_t len) override;
  void EnablePowerAssertionInternal() override;
  void DisablePowerAssertionInternal() override;

private:
  IOBluetoothL2CAPChannel* m_ichan;
  IOBluetoothL2CAPChannel* m_cchan;
  bool m_connected;
  CFRunLoopRef m_wiimote_thread_run_loop;
  IOPMAssertionID m_pm_assertion;
};

class WiimoteDarwinHid final : public Wiimote
{
public:
  WiimoteDarwinHid(IOHIDDeviceRef device);
  ~WiimoteDarwinHid() override;

protected:
  bool ConnectInternal() override;
  void DisconnectInternal() override;
  bool IsConnected() const override;
  void IOWakeup() override;
  int IORead(u8* buf) override;
  int IOWrite(u8 const* buf, size_t len) override;

private:
  static void ReportCallback(void* context, IOReturn result, void* sender, IOHIDReportType type,
                             u32 reportID, u8* report, CFIndex reportLength);
  static void RemoveCallback(void* context, IOReturn result, void* sender);
  void QueueBufferReport(int length);
  IOHIDDeviceRef m_device;
  bool m_connected;
  std::atomic<bool> m_interrupted;
  Report m_report_buffer;
  Common::FifoQueue<Report> m_buffered_reports;
};

class WiimoteScannerDarwin final : public WiimoteScannerBackend
{
public:
  WiimoteScannerDarwin() {}
  ~WiimoteScannerDarwin() {}
  bool IsReady() const override;
  void FindWiimotes(std::vector<Wiimote*>&, Wiimote*&) override;
  void Update() override{};  // not needed
};

class WiimoteScannerDarwinHID final : public WiimoteScannerBackend
{
public:
  WiimoteScannerDarwinHID() {}
  ~WiimoteScannerDarwinHID() {}
  bool IsReady() const override;
  void FindWiimotes(std::vector<Wiimote*>&, Wiimote*&) override;
  void Update() override{};  // not needed
};
}
