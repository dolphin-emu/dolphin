// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace ControllerEmu
{
class AnalogStick;
class ControlGroup;
class Triggers;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class BalanceBoardGroup
{
  Balance,
  Weight
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

  static constexpr float DEFAULT_WEIGHT = 63.5;  // kilograms; no specific meaning to this value

  void BuildDesiredExtensionState(DesiredExtensionState* target_state) override;
  void Update(const DesiredExtensionState& target_state) override;
  void Reset() override;
  void DoState(PointerWrap& p) override;

  ControllerEmu::ControlGroup* GetGroup(BalanceBoardGroup group);

  void LoadDefaults(const ControllerInterface& ciface) override;

  // Use the same calibration data for all sensors.
  // Wii Fit appears to convert to this internally (so it doesn't care about differences smaller
  // than a gram). Normal balance boards tend to be less precise, usually around 10 grams.
  static constexpr u16 WEIGHT_0_KG = 0;
  static constexpr u16 WEIGHT_17_KG = 17000;
  static constexpr u16 WEIGHT_34_KG = 34000;
  // Chosen arbitrarily from the value for pokechu22's board.  As long as the calibration and
  // actual temperatures match, the value here doesn't matter.
  static constexpr u8 TEMPERATURE = 0x19;

private:
  u16 ConvertToSensorWeight(double weight_in_kilos);
  void ComputeCalibrationChecksum();

  ControllerEmu::AnalogStick* m_balance;
  ControllerEmu::Triggers* m_weight;
};
}  // namespace WiimoteEmu
