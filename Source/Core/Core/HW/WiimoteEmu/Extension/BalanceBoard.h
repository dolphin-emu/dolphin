// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace ControllerEmu
{
class ControlGroup;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class BalanceBoardGroup
{
};

class BalanceBoardExt : public Extension1stParty
{
public:
  struct DataFormat
  {
    u16 top_right;
    u16 bottom_right;
    u16 top_left;
    u16 bottom_left;
    u8 temperature;
    u8 padding;
    u8 battery;
  };
  static_assert(sizeof(DataFormat) == 12, "Wrong size");

  BalanceBoardExt();

  void BuildDesiredExtensionState(DesiredExtensionState* target_state) override;
  void Update(const DesiredExtensionState& target_state) override;
  void Reset() override;
  void DoState(PointerWrap& p) override;

  ControllerEmu::ControlGroup* GetGroup(BalanceBoardGroup group);

  void LoadDefaults(const ControllerInterface& ciface) override;

private:
};
}  // namespace WiimoteEmu
