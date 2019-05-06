// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{
class WiimoteScannerDummy final : public WiimoteScannerBackend
{
public:
  WiimoteScannerDummy() = default;
  ~WiimoteScannerDummy() override = default;
  bool IsReady() const override { return false; }
  void FindWiimotes(std::vector<Wiimote*>&, Wiimote*&) override {}
  void Update() override {}
};
}  // namespace WiimoteReal
