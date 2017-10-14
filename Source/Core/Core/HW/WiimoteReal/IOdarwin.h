// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __APPLE__
#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{
class WiimoteScannerDarwin final : public WiimoteScannerBackend
{
public:
  WiimoteScannerDarwin() = default;
  ~WiimoteScannerDarwin() override = default;
  bool IsReady() const override;
  void FindWiimotes(std::vector<Wiimote*>&, Wiimote*&) override;
  void Update() override {}  // not needed
};
}

#else
#include "Core/HW/WiimoteReal/IODummy.h"
namespace WiimoteReal
{
using WiimoteScannerDarwin = WiimoteScannerDummy;
}
#endif
