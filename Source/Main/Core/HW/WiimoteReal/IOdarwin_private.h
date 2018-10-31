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
#include <IOKit/pwr_mgt/IOPMLib.h>

#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{
class WiimoteDarwin final : public Wiimote
{
public:
  WiimoteDarwin(IOBluetoothDevice* device);
  ~WiimoteDarwin() override;
  std::string GetId() const override { return [[m_btd addressString] UTF8String]; }
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
}  // namespace WiimoteReal
