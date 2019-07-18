// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Extension/BalanceBoard.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>

#include "Common/BitUtils.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/Swap.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/Triggers.h"

namespace WiimoteEmu
{
constexpr std::array<u8, 6> balance_board_id{{0x00, 0x00, 0xa4, 0x20, 0x04, 0x02}};

BalanceBoardExt::BalanceBoardExt() : Extension1stParty("BalanceBoard", _trans("Balance Board"))
{
  // balance
  groups.emplace_back(
      m_balance = new ControllerEmu::AnalogStick(
          _trans("Balance"), std::make_unique<ControllerEmu::SquareStickGate>(1.0)));
  groups.emplace_back(m_weight = new ControllerEmu::Triggers(_trans("Weight")));
  m_weight->controls.emplace_back(
      new ControllerEmu::Input(ControllerEmu::Translatability::Translate, _trans("Weight")));
}

void BalanceBoardExt::BuildDesiredExtensionState(DesiredExtensionState* target_state)
{
  const ControllerEmu::AnalogStick::StateData balance_state = m_balance->GetState();
  const ControllerEmu::Triggers::StateData weight_state = m_weight->GetState();
  const auto weight = weight_state.data[0];

  const double total_weight = DEFAULT_WEIGHT * weight;  // kilograms

  const auto top_right = total_weight * (1 + balance_state.x + balance_state.y) / 4;
  const auto bottom_right = total_weight * (1 + balance_state.x - balance_state.y) / 4;
  const auto top_left = total_weight * (1 - balance_state.x + balance_state.y) / 4;
  const auto bottom_left = total_weight * (1 - balance_state.x - balance_state.y) / 4;

  DataFormat bb_data = {};

  bb_data.top_right = Common::swap16(ConvertToSensorWeight(top_right));
  bb_data.bottom_right = Common::swap16(ConvertToSensorWeight(bottom_right));
  bb_data.top_left = Common::swap16(ConvertToSensorWeight(top_left));
  bb_data.bottom_left = Common::swap16(ConvertToSensorWeight(bottom_left));
  bb_data.temperature = TEMPERATURE;
  bb_data.battery = 0x83;  // Above the threshold for 4 bars

  target_state->data = bb_data;
}

void BalanceBoardExt::Update(const DesiredExtensionState& target_state)
{
  DefaultExtensionUpdate<DataFormat>(&m_reg, target_state);
}

void BalanceBoardExt::Reset()
{
  EncryptedExtension::Reset();

  m_reg.identifier = balance_board_id;

  // Build calibration data:
  m_reg.calibration = {{// Unused battery calibration
                        0x01, 0x69, 0x00, 0x00,
                        // Top right 0kg
                        u8(WEIGHT_0_KG >> 8), u8(WEIGHT_0_KG & 0xFF),
                        // Bottom right 0kg
                        u8(WEIGHT_0_KG >> 8), u8(WEIGHT_0_KG & 0xFF),
                        // Top left 0kg
                        u8(WEIGHT_0_KG >> 8), u8(WEIGHT_0_KG & 0xFF),
                        // Bottom left 0kg
                        u8(WEIGHT_0_KG >> 8), u8(WEIGHT_0_KG & 0xFF),
                        // Top right 17kg
                        u8(WEIGHT_17_KG >> 8), u8(WEIGHT_17_KG & 0xFF),
                        // Bottom right 17kg
                        u8(WEIGHT_17_KG >> 8), u8(WEIGHT_17_KG & 0xFF)}};
  m_reg.calibration2 = {{// Top left 17kg
                         u8(WEIGHT_17_KG >> 8), u8(WEIGHT_17_KG & 0xFF),
                         // Bottom left 17kg
                         u8(WEIGHT_17_KG >> 8), u8(WEIGHT_17_KG & 0xFF),
                         // Top right 34kg
                         u8(WEIGHT_34_KG >> 8), u8(WEIGHT_34_KG & 0xFF),
                         // Bottom right 34kg
                         u8(WEIGHT_34_KG >> 8), u8(WEIGHT_34_KG & 0xFF),
                         // Top left 34kg
                         u8(WEIGHT_34_KG >> 8), u8(WEIGHT_34_KG & 0xFF),
                         // Bottom left 34kg
                         u8(WEIGHT_34_KG >> 8), u8(WEIGHT_34_KG & 0xFF),
                         // Checksum - computed later
                         0xff, 0xff, 0xff, 0xff}};
  m_reg.calibration3 = {{TEMPERATURE, 0x01}};

  ComputeCalibrationChecksum();
}

ControllerEmu::ControlGroup* BalanceBoardExt::GetGroup(BalanceBoardGroup group)
{
  switch (group)
  {
  case BalanceBoardGroup::Balance:
    return m_balance;
  case BalanceBoardGroup::Weight:
    return m_weight;
  default:
    assert(false);
    return nullptr;
  }
}

void BalanceBoardExt::DoState(PointerWrap& p)
{
  EncryptedExtension::DoState(p);
}

void BalanceBoardExt::LoadDefaults(const ControllerInterface& ciface)
{
  // Balance
  m_balance->SetControlExpression(0, "I");  // up
  m_balance->SetControlExpression(1, "K");  // down
  m_balance->SetControlExpression(2, "J");  // left
  m_balance->SetControlExpression(3, "L");  // right

  // Because our defaults use keyboard input, set calibration shape to a square.
  m_balance->SetCalibrationFromGate(ControllerEmu::SquareStickGate(.5));
}

u16 BalanceBoardExt::ConvertToSensorWeight(double weight_in_kilos)
{
  constexpr u16 LOW_WEIGHT_DELTA = WEIGHT_17_KG - WEIGHT_0_KG;
  constexpr u16 HIGH_WEIGHT_DELTA = WEIGHT_34_KG - WEIGHT_17_KG;

  // Note: this is the weight on a single sensor, so these ranges make more sense
  // (if all sensors read 34 kilos, then the overall weight would be 136 kilos or 300 pounds...)
  if (weight_in_kilos < 17)
  {
    return WEIGHT_0_KG + (LOW_WEIGHT_DELTA * weight_in_kilos / 17);
  }
  else
  {
    return WEIGHT_17_KG + (HIGH_WEIGHT_DELTA * (weight_in_kilos - 17) / 17);
  }
}

void BalanceBoardExt::ComputeCalibrationChecksum()
{
  std::array<u8, 0x1c> data;

  data[0x00] = m_reg.calibration[0x4];
  data[0x01] = m_reg.calibration[0x5];
  data[0x02] = m_reg.calibration[0x6];
  data[0x03] = m_reg.calibration[0x7];
  data[0x04] = m_reg.calibration[0x8];
  data[0x05] = m_reg.calibration[0x9];
  data[0x06] = m_reg.calibration[0xa];
  data[0x07] = m_reg.calibration[0xb];
  data[0x08] = m_reg.calibration[0xc];
  data[0x09] = m_reg.calibration[0xd];
  data[0x0a] = m_reg.calibration[0xe];
  data[0x0b] = m_reg.calibration[0xf];
  data[0x0c] = m_reg.calibration2[0x0];
  data[0x0d] = m_reg.calibration2[0x1];
  data[0x0e] = m_reg.calibration2[0x2];
  data[0x0f] = m_reg.calibration2[0x3];
  data[0x10] = m_reg.calibration2[0x4];
  data[0x11] = m_reg.calibration2[0x5];
  data[0x12] = m_reg.calibration2[0x6];
  data[0x13] = m_reg.calibration2[0x7];
  data[0x14] = m_reg.calibration2[0x8];
  data[0x15] = m_reg.calibration2[0x9];
  data[0x16] = m_reg.calibration2[0xa];
  data[0x17] = m_reg.calibration2[0xb];
  data[0x18] = m_reg.calibration[0];
  data[0x19] = m_reg.calibration[1];
  data[0x1a] = m_reg.calibration3[0];
  data[0x1b] = m_reg.calibration3[1];

  constexpr u32 CRC_POLYNOMIAL = 0xEDB88320;  // Reversed

  u32 result = 0xffffffff;
  for (auto byte : data)
  {
    // This could be cached into a table (and usually is), but since this is called infrequently and
    // on a small number of bytes, don't bother.
    u32 remainder = (byte ^ (result & 0xff));
    for (auto bit = 0; bit < 8; bit++)
    {
      if (remainder & 1)
        remainder = (remainder >> 1) ^ CRC_POLYNOMIAL;
      else
        remainder = remainder >> 1;
    }

    result = (result >> 8) ^ remainder;
  }
  result ^= 0xffffffff;

  m_reg.calibration2[0x0c] = u8(result >> 24);
  m_reg.calibration2[0x0d] = u8(result >> 16);
  m_reg.calibration2[0x0e] = u8(result >> 8);
  m_reg.calibration2[0x0f] = u8(result);
}
}  // namespace WiimoteEmu
