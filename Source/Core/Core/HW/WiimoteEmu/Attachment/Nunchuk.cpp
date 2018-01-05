// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"

#include <array>
#include <cassert>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Core/HW/WiimoteEmu/HydraTLayer.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"

namespace WiimoteEmu
{
constexpr std::array<u8, 6> nunchuk_id{{0x00, 0x00, 0xa4, 0x20, 0x00, 0x00}};

constexpr std::array<u8, 2> nunchuk_button_bitmasks{{
    Nunchuk::BUTTON_C, Nunchuk::BUTTON_Z,
}};

Nunchuk::Nunchuk(ExtensionReg& reg, int index) : Attachment(_trans("Nunchuk"), reg), m_index(index)
{
  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  m_buttons->controls.emplace_back(new ControllerEmu::Input("C"));
  m_buttons->controls.emplace_back(new ControllerEmu::Input("Z"));

  // stick
  groups.emplace_back(
      m_stick = new ControllerEmu::AnalogStick(_trans("Stick"), DEFAULT_ATTACHMENT_STICK_RADIUS));

  // swing
  groups.emplace_back(m_swing = new ControllerEmu::Force(_trans("Swing")));

  // tilt
  groups.emplace_back(m_tilt = new ControllerEmu::Tilt(_trans("Tilt")));

  // shake
  groups.emplace_back(m_shake = new ControllerEmu::Buttons(_trans("Shake")));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  m_shake->controls.emplace_back(new ControllerEmu::Input(_trans("X")));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  m_shake->controls.emplace_back(new ControllerEmu::Input(_trans("Y")));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  m_shake->controls.emplace_back(new ControllerEmu::Input(_trans("Z")));

  m_id = nunchuk_id;
}

void Nunchuk::GetState(u8* const data)
{
  wm_nc* const ncdata = reinterpret_cast<wm_nc* const>(data);
  ncdata->bt.hex = 0;

  // stick
  double jx, jy;
  m_stick->GetState(&jx, &jy);

  ncdata->jx = u8(STICK_CENTER + jx * STICK_RADIUS);
  ncdata->jy = u8(STICK_CENTER + jy * STICK_RADIUS);

  // Some terribly coded games check whether to move with a check like
  //
  //     if (x != 0 && y != 0)
  //         do_movement(x, y);
  //
  // With keyboard controls, these games break if you simply hit
  // of the axes. Adjust this if you're hitting one of the axes so that
  // we slightly tweak the other axis.
  if (ncdata->jx != STICK_CENTER || ncdata->jy != STICK_CENTER)
  {
    if (ncdata->jx == STICK_CENTER)
      ++ncdata->jx;
    if (ncdata->jy == STICK_CENTER)
      ++ncdata->jy;
  }

  HydraTLayer::GetNunchuk(m_index, &ncdata->jx, &ncdata->jy, &ncdata->bt);

  AccelData accel;

  // tilt
  EmulateTilt(&accel, m_tilt);
  HydraTLayer::GetNunchukAcceleration(m_index, &accel);

  // swing
  EmulateSwing(&accel, m_swing);
  // shake
  EmulateShake(&accel, m_shake, m_shake_step.data());
  // buttons
  m_buttons->GetState(&ncdata->bt.hex, nunchuk_button_bitmasks.data());

  // flip the button bits :/
  ncdata->bt.hex ^= 0x03;

  // We now use 2 bits more precision, so multiply by 4 before converting to int
  s16 accel_x = (s16)(4 * (accel.x * ACCEL_RANGE + ACCEL_ZERO_G));
  s16 accel_y = (s16)(4 * (accel.y * ACCEL_RANGE + ACCEL_ZERO_G));
  s16 accel_z = (s16)(4 * (accel.z * ACCEL_RANGE + ACCEL_ZERO_G));

  accel_x = MathUtil::Clamp<s16>(accel_x, 0, 1024);
  accel_y = MathUtil::Clamp<s16>(accel_y, 0, 1024);
  accel_z = MathUtil::Clamp<s16>(accel_z, 0, 1024);

  ncdata->ax = (accel_x >> 2) & 0xFF;
  ncdata->ay = (accel_y >> 2) & 0xFF;
  ncdata->az = (accel_z >> 2) & 0xFF;
  ncdata->bt.acc_x_lsb = accel_x & 0x3;
  ncdata->bt.acc_y_lsb = accel_y & 0x3;
  ncdata->bt.acc_z_lsb = accel_z & 0x3;
}

bool Nunchuk::IsButtonPressed() const
{
  u8 buttons = 0;
  m_buttons->GetState(&buttons, nunchuk_button_bitmasks.data());
  return buttons != 0;
}

ControllerEmu::ControlGroup* Nunchuk::GetGroup(NunchukGroup group)
{
  switch (group)
  {
  case NunchukGroup::Buttons:
    return m_buttons;
  case NunchukGroup::Stick:
    return m_stick;
  case NunchukGroup::Tilt:
    return m_tilt;
  case NunchukGroup::Swing:
    return m_swing;
  case NunchukGroup::Shake:
    return m_shake;
  default:
    assert(false);
    return nullptr;
  }
}

void Nunchuk::LoadDefaults(const ControllerInterface& ciface)
{
  // Stick
  m_stick->SetControlExpression(0, "W");  // up
  m_stick->SetControlExpression(1, "S");  // down
  m_stick->SetControlExpression(2, "A");  // left
  m_stick->SetControlExpression(3, "D");  // right

// Buttons
#ifdef _WIN32
  m_buttons->SetControlExpression(0, "LCONTROL");  // C
  m_buttons->SetControlExpression(1, "LSHIFT");    // Z
#elif __APPLE__
  m_buttons->SetControlExpression(0, "Left Control");  // C
  m_buttons->SetControlExpression(1, "Left Shift");    // Z
#else
  m_buttons->SetControlExpression(0, "Control_L");  // C
  m_buttons->SetControlExpression(1, "Shift_L");    // Z
#endif
}
}
