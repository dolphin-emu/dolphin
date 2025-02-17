// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace WiimoteEmu
{
class BalanceBoard;
enum class BalanceBoardGroup;

// NOTE: Although the balance board is implemented as an extension (which matches how the actual
// balance board works), the actual controls are not registered in the same way as other extensions;
// instead, WiimoteEmu::BalanceBoard has all of the controls and some are also used by the
// extension.
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

  static constexpr const char* BALANCE_GROUP = "Balance";

  static constexpr const char* TOP_RIGHT_SENSOR = "TR";
  static constexpr const char* BOTTOM_RIGHT_SENSOR = "BR";
  static constexpr const char* TOP_LEFT_SENSOR = "TL";
  static constexpr const char* BOTTOM_LEFT_SENSOR = "BL";

  BalanceBoardExt(BalanceBoard* owner);

  static constexpr float DEFAULT_WEIGHT = 63.5;  // kilograms; no specific meaning to this value

  void BuildDesiredExtensionState(DesiredExtensionState* target_state) override;
  void Update(const DesiredExtensionState& target_state) override;
  void Reset() override;
  void DoState(PointerWrap& p) override;

  static u16 ConvertToSensorWeight(double weight_in_kilos);
  static double ConvertToKilograms(u16 sensor_weight);

  // Use the same calibration data for all sensors.
  // Wii Fit internally converts to grams, but using grams for the actual values leads to
  // overflowing values, and also underflowing values when a sensor gets negative if balance is
  // extremely tilted. Actual balance boards tend to have a sensitivity of about 10 grams.
  static constexpr u16 WEIGHT_0_KG = 10000;
  static constexpr u16 WEIGHT_17_KG = 11700;
  static constexpr u16 WEIGHT_34_KG = 13400;

  // Chosen arbitrarily from the value for pokechu22's board.  As long as the calibration and
  // actual temperatures match, the value here doesn't matter.
  static constexpr u8 TEMPERATURE = 0x19;

private:
  void ComputeCalibrationChecksum();

  static constexpr u16 LOW_WEIGHT_DELTA = WEIGHT_17_KG - WEIGHT_0_KG;
  static constexpr u16 HIGH_WEIGHT_DELTA = WEIGHT_34_KG - WEIGHT_17_KG;

  const BalanceBoard* m_owner;
};
}  // namespace WiimoteEmu
