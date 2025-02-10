// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Matrix.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace WiimoteEmu
{
class BalanceBoard;

// NOTE: Although the balance board is implemented as an extension (which matches how the actual
// balance board works), actual controls are not registered in the same way as other extensions;
// WiimoteEmu::BalanceBoard has all of the controls and some are also used by the extension.
class BalanceBoardExt : public Extension1stParty
{
public:
  struct DataFormat
  {
    // top-right, bottom-right, top-left, bottom-left
    u16 sensor_tr;
    u16 sensor_br;
    u16 sensor_tl;
    u16 sensor_bl;
    // Note: temperature, padding, and battery bytes are not included.
    //  temperature change is not exposed, and arguably not important.
    //  battery level is pulled from the wii remote state.
  };
  static_assert(sizeof(DataFormat) == 8, "Wrong size");

  static constexpr const char* BALANCE_GROUP = "Balance";

  static constexpr const char* SENSOR_TR = "TR";
  static constexpr const char* SENSOR_BR = "BR";
  static constexpr const char* SENSOR_TL = "TL";
  static constexpr const char* SENSOR_BL = "BL";

  BalanceBoardExt(BalanceBoard* owner);

  static constexpr float DEFAULT_WEIGHT = 60;

  void BuildDesiredExtensionState(DesiredExtensionState* target_state) override;
  void Update(const DesiredExtensionState& target_state) override;
  void Reset() override;

  // Vec4 ordered: top-right, bottom-right, top-left, bottom-left
  static Common::DVec2 SensorsToCenterOfBalance(Common::DVec4 sensors);
  static Common::DVec4 CenterOfBalanceToSensors(Common::DVec2 balance, double total_weight);

  static u16 ConvertToSensorWeight(double weight_in_kilos);
  static double ConvertToKilograms(u16 sensor_weight);

private:
  void ComputeCalibrationChecksum();

  const BalanceBoard* m_owner;
};
}  // namespace WiimoteEmu
