// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"

namespace WiimoteEmu
{
constexpr std::array<u8, 6> balance_board_id{{0x00, 0x00, 0xa4, 0x20, 0x04, 0x02}};

BalanceBoard::BalanceBoard() : Extension1stParty("BalanceBoard", _trans("Balance Board"))
{
}

void BalanceBoard::Update()
{
  DataFormat bb_data = {};

  bb_data.top_right = Common::swap16(0x0743);
  bb_data.bottom_right = Common::swap16(0x11a2);
  bb_data.top_left = Common::swap16(0x06a8);
  bb_data.bottom_left = Common::swap16(0x4694);
  bb_data.temperature = 0x19;
  bb_data.battery = 0x83;

  Common::BitCastPtr<DataFormat>(&m_reg.controller_data) = bb_data;
}

void BalanceBoard::Reset()
{
  EncryptedExtension::Reset();

  m_reg.identifier = balance_board_id;

  // Build calibration data:
  m_reg.calibration = {{// Unused battery calibration
                        0x01, 0x69, 0x00, 0x00,
                        // Top right 0kg
                        0x07, 0xbc,
                        // Bottom right 0kg
                        0x11, 0x8b,
                        // Top left 0kg
                        0x06, 0xba,
                        // Bottom left 0kg
                        0x46, 0x52,
                        // Top right 17kg
                        0x0e, 0x6e,
                        // Bottom right 17kg
                        0x18, 0x79}};
  m_reg.calibration2 = {{// Top left 17kg
                         0x0d, 0x5d,
                         // Bottom left 17kg
                         0x4d, 0x4c,
                         // Top right 34kg
                         0x15, 0x2e,
                         // Bottom right 34kg
                         0x1f, 0x71,
                         // Top left 34kg
                         0x14, 0x07,
                         // Bottom left 34kg
                         0x54, 0x51,
                         // Checksum
                         0xa9, 0x06, 0xb4, 0xf0}};
  m_reg.calibration3 = {{0x19, 0x01}};

  // UpdateCalibrationDataChecksum(m_reg.calibration, CALIBRATION_CHECKSUM_BYTES);
}

ControllerEmu::ControlGroup* BalanceBoard::GetGroup(BalanceBoardGroup group)
{
  return nullptr;
}

void BalanceBoard::DoState(PointerWrap& p)
{
  EncryptedExtension::DoState(p);
}

void BalanceBoard::LoadDefaults(const ControllerInterface& ciface)
{
}

}  // namespace WiimoteEmu
