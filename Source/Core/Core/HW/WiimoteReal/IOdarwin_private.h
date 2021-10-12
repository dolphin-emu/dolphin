// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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
