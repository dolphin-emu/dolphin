// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Extension/DrawsomeTablet.h"

#include <array>

#include "Common/Assert.h"
#include "Common/BitUtils.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Triggers.h"

namespace WiimoteEmu
{
constexpr std::array<u8, 6> drawsome_tablet_id{{0xff, 0x00, 0xa4, 0x20, 0x00, 0x13}};

// i18n: The "Drawsome" (combination of "Draw" and "Awesome") tablet wiimote extension by Ubisoft.
DrawsomeTablet::DrawsomeTablet() : Extension3rdParty("Drawsome", _trans("Drawsome Tablet"))
{
  // Stylus
  groups.emplace_back(m_stylus = new ControllerEmu::AnalogStick(
                          _trans("Stylus"), std::make_unique<ControllerEmu::SquareStickGate>(1.0)));

  // Touch
  groups.emplace_back(m_touch = new ControllerEmu::Triggers(_trans("Touch")));
  m_touch->AddInput(ControllerEmu::Translate, _trans("Pressure"));
  m_touch->AddInput(ControllerEmu::Translate, _trans("Lift"));
}

void DrawsomeTablet::BuildDesiredExtensionState(DesiredExtensionState* target_state)
{
  DataFormat& tablet_data = target_state->data.emplace<DataFormat>();

  // Stylus X/Y (calibrated values):
  constexpr u16 MIN_X = 0x0000;
  constexpr u16 MAX_X = 0x27ff;
  // Note: While 0x15ff seems to be the ideal calibrated value,
  // the "Drawsome" game expects you to go "off screen" a bit to access some menu items.
  constexpr u16 MIN_Y = 0x15ff + 0x100;
  constexpr u16 MAX_Y = 0x00;
  constexpr u16 CENTER_X = (MAX_X + MIN_X + 1) / 2;
  constexpr u16 CENTER_Y = (MAX_Y + MIN_Y + 1) / 2;

  const auto stylus_state = m_stylus->GetState(m_input_override_function);
  const u16 stylus_x = MapFloat<u16>(stylus_state.x, CENTER_X, MIN_X, MAX_X);
  const u16 stylus_y = MapFloat<u16>(-stylus_state.y, CENTER_Y, MAX_Y, MIN_Y);

  tablet_data.stylus_x1 = u8(stylus_x);
  tablet_data.stylus_x2 = u8(stylus_x >> 8);

  tablet_data.stylus_y1 = u8(stylus_y);
  tablet_data.stylus_y2 = u8(stylus_y >> 8);

  const auto touch_state = m_touch->GetState(m_input_override_function);

  // Note: Pen X/Y holds the last value when the pen is lifted.
  const bool is_stylus_lifted = std::lround(touch_state.data[1]) != 0;

  constexpr u8 NEUTRAL_STATUS = 0x8;
  constexpr u8 PEN_LIFTED_BIT = 0x10;

  u8 status = NEUTRAL_STATUS;

  if (is_stylus_lifted)
    status |= PEN_LIFTED_BIT;

  tablet_data.status = status;

  // Pressure (0 - 0x7ff):
  constexpr u16 MAX_PRESSURE = 0x7ff;

  const u16 pressure = MapFloat<u16>(touch_state.data[0], 0, 0, MAX_PRESSURE);

  tablet_data.pressure1 = u8(pressure);
  tablet_data.pressure2 = u8(pressure >> 8);
}

void DrawsomeTablet::Update(const DesiredExtensionState& target_state)
{
  DefaultExtensionUpdate<DataFormat>(&m_reg, target_state);
}

void DrawsomeTablet::Reset()
{
  EncryptedExtension::Reset();

  m_reg.identifier = drawsome_tablet_id;

  // Assuming calibration data is 0xff filled.
  m_reg.calibration.fill(0xff);
}

ControllerEmu::ControlGroup* DrawsomeTablet::GetGroup(DrawsomeTabletGroup group)
{
  switch (group)
  {
  case DrawsomeTabletGroup::Stylus:
    return m_stylus;
  case DrawsomeTabletGroup::Touch:
    return m_touch;
  default:
    ASSERT(false);
    return nullptr;
  }
}

}  // namespace WiimoteEmu
