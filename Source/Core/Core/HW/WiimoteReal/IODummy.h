// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
  void RequestStopSearching() override {}
};
}  // namespace WiimoteReal
