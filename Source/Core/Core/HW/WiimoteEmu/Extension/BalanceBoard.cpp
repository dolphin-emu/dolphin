// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Extension/BalanceBoard.h"

#include <algorithm>
#include <array>
#include <cstring>

#include <zlib.h>

#include "Common/BitUtils.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/Swap.h"

#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Triggers.h"

namespace WiimoteEmu
{
constexpr std::array<u8, 6> balance_board_id{{0x00, 0x00, 0xa4, 0x20, 0x04, 0x02}};

BalanceBoardExt::BalanceBoardExt(BalanceBoard* owner)
    : Extension1stParty("BalanceBoard", _trans("Balance Board")), m_owner(owner)
{
}

void BalanceBoardExt::BuildDesiredExtensionState(DesiredExtensionState* target_state)
{
  const ControllerEmu::AnalogStick::StateData balance_state = m_owner->m_balance->GetState();
  const ControllerEmu::Triggers::StateData weight_state = m_owner->m_weight->GetState();
  const auto weight = weight_state.data[0];

  const double total_weight = DEFAULT_WEIGHT * weight;  // kilograms

  auto top_right = total_weight * (1 + balance_state.x + balance_state.y) / 4;
  auto bottom_right = total_weight * (1 + balance_state.x - balance_state.y) / 4;
  auto top_left = total_weight * (1 - balance_state.x + balance_state.y) / 4;
  auto bottom_left = total_weight * (1 - balance_state.x - balance_state.y) / 4;

  if (m_input_override_function)
  {
    if (const std::optional<ControlState> top_right_override =
            m_input_override_function(BALANCE_GROUP, TOP_RIGHT_SENSOR, top_right))
    {
      top_right = *top_right_override;
    }
    if (const std::optional<ControlState> bottom_right_override =
            m_input_override_function(BALANCE_GROUP, BOTTOM_RIGHT_SENSOR, bottom_right))
    {
      bottom_right = *bottom_right_override;
    }
    if (const std::optional<ControlState> top_left_override =
            m_input_override_function(BALANCE_GROUP, TOP_LEFT_SENSOR, top_left))
    {
      top_left = *top_left_override;
    }
    if (const std::optional<ControlState> bottom_left_override =
            m_input_override_function(BALANCE_GROUP, BOTTOM_LEFT_SENSOR, bottom_left))
    {
      bottom_left = *bottom_left_override;
    }
  }

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

void BalanceBoardExt::DoState(PointerWrap& p)
{
  EncryptedExtension::DoState(p);
}

u16 BalanceBoardExt::ConvertToSensorWeight(double weight_in_kilos)
{
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

double BalanceBoardExt::ConvertToKilograms(u16 sensor_weight)
{
  if (sensor_weight < WEIGHT_17_KG)
  {
    return (sensor_weight - WEIGHT_0_KG) * 17. / LOW_WEIGHT_DELTA;
  }
  else
  {
    return (sensor_weight - WEIGHT_17_KG) * 17. / HIGH_WEIGHT_DELTA + 17.;
  }
}

void BalanceBoardExt::ComputeCalibrationChecksum()
{
  u32 crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, &m_reg.calibration[4], 0xc);   // Skip the first 4 bytes
  crc = crc32(crc, &m_reg.calibration2[0], 0xc);  // Skip the last 4 bytes (the CRC itself)
  crc = crc32(crc, &m_reg.calibration[0], 2);     // Hash 2 of the bytes skipped earlier
  crc = crc32(crc, &m_reg.calibration3[0], 2);

  m_reg.calibration2[0x0c] = u8(crc >> 24);
  m_reg.calibration2[0x0d] = u8(crc >> 16);
  m_reg.calibration2[0x0e] = u8(crc >> 8);
  m_reg.calibration2[0x0f] = u8(crc);
}
}  // namespace WiimoteEmu
