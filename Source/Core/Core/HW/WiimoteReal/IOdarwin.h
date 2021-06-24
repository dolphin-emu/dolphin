// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __APPLE__
#include "Core/HW/WiimoteReal/WiimoteReal.h"

#ifdef __OBJC__
#import <IOBluetooth/IOBluetooth.h>
#else
// IOBluetooth's types won't be defined in pure C++ mode.
typedef void IOBluetoothHostController;
#endif

namespace WiimoteReal
{
class WiimoteScannerDarwin final : public WiimoteScannerBackend
{
public:
  WiimoteScannerDarwin();
  ~WiimoteScannerDarwin() override;
  bool IsReady() const override;
  void FindWiimotes(std::vector<Wiimote*>&, Wiimote*&) override;
  void Update() override {}  // not needed
  void RequestStopSearching() override { m_stop_scanning = true; }
private:
  IOBluetoothHostController* m_host_controller;
  bool m_stop_scanning = false;
};
}  // namespace WiimoteReal

#else
#include "Core/HW/WiimoteReal/IODummy.h"
namespace WiimoteReal
{
using WiimoteScannerDarwin = WiimoteScannerDummy;
}
#endif
