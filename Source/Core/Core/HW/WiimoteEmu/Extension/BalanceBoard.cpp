// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Extension/BalanceBoard.h"

#include <array>
#include <cstring>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Hash.h"
#include "Common/Swap.h"

#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"

namespace WiimoteEmu
{
constexpr std::array<u8, 6> balance_board_id{{0x00, 0x00, 0xa4, 0x20, 0x04, 0x02}};

BalanceBoardExt::BalanceBoardExt(BalanceBoard* owner)
    : Extension1stParty("BalanceBoard", _trans("Balance Board")), m_owner(owner)
{
}

// Use the same calibration data for all sensors.
// Wii Fit internally converts to grams, but using grams for the actual values leads to
// overflowing values, and also underflowing values when a sensor gets negative if balance is
// extremely tilted. Actual balance boards tend to have a sensitivity of about 10 grams.

// Real board values vary greatly but these nice values are very near those of a real board.
static constexpr u16 KG17_RANGE = 1700;
static constexpr u16 CALIBRATED_0_KG = 10000;
static constexpr u16 CALIBRATED_17_KG = CALIBRATED_0_KG + KG17_RANGE;
static constexpr u16 CALIBRATED_34_KG = CALIBRATED_17_KG + KG17_RANGE;

// Chosen arbitrarily from the value for pokechu22's board.  As long as the calibration and
// actual temperatures match, the value here doesn't matter.
static constexpr u8 REFERENCE_TEMPERATURE = 0x19;

void BalanceBoardExt::BuildDesiredExtensionState(DesiredExtensionState* target_state)
{
  ControllerEmu::AnalogStick::StateData balance_state = m_owner->m_balance->GetState();

  // TODO: make this all less ugly and work better..

  const double half_body_weight = m_owner->m_weight_setting.GetValue() * 0.5;

  const double foot_l = m_owner->m_left_foot->controls[0]->GetState() * 2;
  const double foot_r = m_owner->m_right_foot->controls[0]->GetState() * 2;

  const double down_l = std::abs(1 - foot_l);
  const double down_r = std::abs(1 - foot_r);

  balance_state.x += (1 - down_l) - (1 - down_r);

  // What percent of weight user is putting on each foot.
  const double amt_l = std::clamp(1 - balance_state.x, 0.0, down_l * 2);
  const double amt_r = std::clamp(1 + balance_state.x, 0.0, down_r * 2);

  // How much the foot is on the board.
  const double on_board_l = std::max(1 - foot_l, 0.0);
  const double on_board_r = std::max(1 - foot_r, 0.0);

  // What percent of weight each foot is putting on the board.
  const double weight_l = amt_l * on_board_l;
  const double weight_r = amt_r * on_board_r;

  INFO_LOG_FMT(WIIMOTE, "f: {:.2f} {:.2f} b: {:.2f} {:.2f} w: {:.2f} {:.2f}", amt_l, amt_r,
               on_board_l, on_board_r, weight_l, weight_r);

  // Distance between the feet on the bottom of the board.
  constexpr auto SENSOR_DISTANCE = Common::DVec2{43, 24};

  const auto balance_scale = Common::DVec2{m_owner->m_foot_separation_setting.GetValue(),
                                           m_owner->m_foot_length_setting.GetValue()} /
                             SENSOR_DISTANCE;

  const double weight = (weight_l + weight_r);
  balance_state.x = weight ? (weight_r - weight_l) / weight : 0.0;

  auto [weight_tr, weight_br, weight_tl, weight_bl] =
      CenterOfBalanceToSensors(balance_state * balance_scale, weight * half_body_weight).data;

  if (auto& func = m_input_override_function)
  {
    weight_tr = func(BALANCE_GROUP, SENSOR_TR, weight_tr).value_or(weight_tr);
    weight_br = func(BALANCE_GROUP, SENSOR_BR, weight_br).value_or(weight_br);
    weight_tl = func(BALANCE_GROUP, SENSOR_TL, weight_tl).value_or(weight_tl);
    weight_bl = func(BALANCE_GROUP, SENSOR_BL, weight_bl).value_or(weight_bl);
  }

  DataFormat bb_data = {};

  bb_data.sensor_tr = Common::swap16(ConvertToSensorWeight(weight_tr));
  bb_data.sensor_br = Common::swap16(ConvertToSensorWeight(weight_br));
  bb_data.sensor_tl = Common::swap16(ConvertToSensorWeight(weight_tl));
  bb_data.sensor_bl = Common::swap16(ConvertToSensorWeight(weight_bl));

  target_state->data = bb_data;
}

void BalanceBoardExt::Update(const DesiredExtensionState& target_state)
{
  DefaultExtensionUpdate<DataFormat>(&m_reg, target_state);

  m_reg.controller_data[0x8] = REFERENCE_TEMPERATURE;
  m_reg.controller_data[0x9] = 0x00;

  // FYI: Real EXT battery byte doesn't exactly match status report battery byte.
  //  e.g. Seen: EXT:0x9e and Status:0xc6
  //  I'm guessing they just have separate ADCs.
  m_reg.controller_data[0xa] = m_owner->m_status.battery;
}

void BalanceBoardExt::Reset()
{
  EncryptedExtension::Reset();

  m_reg.identifier = balance_board_id;

  struct CalibrationData
  {
    u8 always_0x01 = 0x01;
    u8 battery = REFERENCE_BATTERY;
    u8 always_0x00[2] = {};

    using SensorValue = Common::BigEndianValue<u16>;

    // Each array is ordered: TR, BR, TL, BL
    std::array<SensorValue, 4> kg0;
    std::array<SensorValue, 4> kg17;
    std::array<SensorValue, 4> kg34;

    // Note that 4 checksum bytes immediately follow.
  };
  static_assert(sizeof(CalibrationData) == 0x1c, "Wrong size");

  CalibrationData cal_data{};
  cal_data.kg0.fill(CalibrationData::SensorValue{CALIBRATED_0_KG});
  cal_data.kg17.fill(CalibrationData::SensorValue{CALIBRATED_17_KG});
  cal_data.kg34.fill(CalibrationData::SensorValue{CALIBRATED_34_KG});

  Common::BitCastPtr<CalibrationData>(m_reg.calibration_long.data()) = cal_data;
  m_reg.calibration2 = {REFERENCE_TEMPERATURE, 0x01};

  ComputeCalibrationChecksum();
}

u16 BalanceBoardExt::ConvertToSensorWeight(double weight_in_kilos)
{
  return ControllerEmu::MapFloat((weight_in_kilos - 17.0) / 17.0, CALIBRATED_17_KG, CALIBRATED_0_KG,
                                 CALIBRATED_34_KG);
}

double BalanceBoardExt::ConvertToKilograms(u16 sensor_weight)
{
  const auto result = ControllerEmu::MapToFloat<double>(sensor_weight, CALIBRATED_17_KG,
                                                        CALIBRATED_0_KG, CALIBRATED_34_KG);
  return result * 17.0 + 17.0;
}

void BalanceBoardExt::ComputeCalibrationChecksum()
{
  u32 crc = Common::StartCRC32();
  // Skip the first 4 bytes and the last 4 bytes (the CRC itself)
  crc = Common::UpdateCRC32(crc, &m_reg.calibration_long[4], 0x18);
  // Hash 2 of the bytes skipped earlier
  crc = Common::UpdateCRC32(crc, m_reg.calibration_long.data(), 2);
  crc = Common::UpdateCRC32(crc, m_reg.calibration2.data(), 2);

  const Common::BigEndianValue result{crc};
  Common::BitCastPtr<Common::BigEndianValue<u32>>(&m_reg.calibration_long[0x1c]) = result;
}

Common::DVec2 BalanceBoardExt::SensorsToCenterOfBalance(Common::DVec4 sensors)
{
  const double right = sensors.data[0] + sensors.data[1];
  const double left = sensors.data[2] + sensors.data[3];

  const double total = right + left;
  if (!total)
    return Common::DVec2{};

  const double top = sensors.data[0] + sensors.data[2];
  const double bottom = sensors.data[1] + sensors.data[3];
  return Common::DVec2(right - left, top - bottom) / total;
}

Common::DVec4 BalanceBoardExt::CenterOfBalanceToSensors(Common::DVec2 balance, double total_weight)
{
  return Common::DVec4{(1 + balance.x) * (1 + balance.y), (1 + balance.x) * (1 - balance.y),
                       (1 - balance.x) * (1 + balance.y), (1 - balance.x) * (1 - balance.y)} *
         (total_weight * 0.25);
}

}  // namespace WiimoteEmu
