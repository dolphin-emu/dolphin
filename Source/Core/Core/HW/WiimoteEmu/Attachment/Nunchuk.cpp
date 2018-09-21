// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"

#include <array>
#include <cassert>
#include <cstring>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Core/Config/WiimoteInputSettings.h"
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
    Nunchuk::BUTTON_C,
    Nunchuk::BUTTON_Z,
}};

Nunchuk::Nunchuk(ExtensionReg& reg) : Attachment(_trans("Nunchuk"), reg)
{
  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  m_buttons->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "C"));
  m_buttons->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Z"));

  // stick
  groups.emplace_back(
      m_stick = new ControllerEmu::AnalogStick(_trans("Stick"), DEFAULT_ATTACHMENT_STICK_RADIUS));

  // swing
  groups.emplace_back(m_swing = new ControllerEmu::Force(_trans("Swing")));
  groups.emplace_back(m_swing_slow = new ControllerEmu::Force("SwingSlow"));
  groups.emplace_back(m_swing_fast = new ControllerEmu::Force("SwingFast"));

  // tilt
  groups.emplace_back(m_tilt = new ControllerEmu::Tilt(_trans("Tilt")));

  // shake
  groups.emplace_back(m_shake = new ControllerEmu::Buttons(_trans("Shake")));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  m_shake->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::Translate, _trans("X")));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  m_shake->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::Translate, _trans("Y")));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  m_shake->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::Translate, _trans("Z")));

  groups.emplace_back(m_shake_soft = new ControllerEmu::Buttons("ShakeSoft"));
  m_shake_soft->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "X"));
  m_shake_soft->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Y"));
  m_shake_soft->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Z"));

  groups.emplace_back(m_shake_hard = new ControllerEmu::Buttons("ShakeHard"));
  m_shake_hard->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "X"));
  m_shake_hard->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Y"));
  m_shake_hard->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Z"));

  m_id = nunchuk_id;
}

void Nunchuk::GetState(u8* const data)
{
  wm_nc nc_data = {};

  // stick
  const ControllerEmu::AnalogStick::StateData stick_state = m_stick->GetState();
  nc_data.jx = u8(STICK_CENTER + stick_state.x * STICK_RADIUS);
  nc_data.jy = u8(STICK_CENTER + stick_state.y * STICK_RADIUS);

  // Some terribly coded games check whether to move with a check like
  //
  //     if (x != 0 && y != 0)
  //         do_movement(x, y);
  //
  // With keyboard controls, these games break if you simply hit
  // of the axes. Adjust this if you're hitting one of the axes so that
  // we slightly tweak the other axis.
  if (nc_data.jx != STICK_CENTER || nc_data.jy != STICK_CENTER)
  {
    if (nc_data.jx == STICK_CENTER)
      ++nc_data.jx;
    if (nc_data.jy == STICK_CENTER)
      ++nc_data.jy;
  }

  AccelData accel;

  // tilt
  EmulateTilt(&accel, m_tilt);

  // swing
  EmulateSwing(&accel, m_swing, Config::Get(Config::NUNCHUK_INPUT_SWING_INTENSITY_MEDIUM));
  EmulateSwing(&accel, m_swing_slow, Config::Get(Config::NUNCHUK_INPUT_SWING_INTENSITY_SLOW));
  EmulateSwing(&accel, m_swing_fast, Config::Get(Config::NUNCHUK_INPUT_SWING_INTENSITY_FAST));

  // shake
  EmulateShake(&accel, m_shake, Config::Get(Config::NUNCHUK_INPUT_SHAKE_INTENSITY_MEDIUM),
               m_shake_step.data());
  EmulateShake(&accel, m_shake_soft, Config::Get(Config::NUNCHUK_INPUT_SHAKE_INTENSITY_SOFT),
               m_shake_soft_step.data());
  EmulateShake(&accel, m_shake_hard, Config::Get(Config::NUNCHUK_INPUT_SHAKE_INTENSITY_HARD),
               m_shake_hard_step.data());

  // buttons
  m_buttons->GetState(&nc_data.bt.hex, nunchuk_button_bitmasks.data());

  // flip the button bits :/
  nc_data.bt.hex ^= 0x03;

  // We now use 2 bits more precision, so multiply by 4 before converting to int
  s16 accel_x = (s16)(4 * (accel.x * ACCEL_RANGE + ACCEL_ZERO_G));
  s16 accel_y = (s16)(4 * (accel.y * ACCEL_RANGE + ACCEL_ZERO_G));
  s16 accel_z = (s16)(4 * (accel.z * ACCEL_RANGE + ACCEL_ZERO_G));

  accel_x = MathUtil::Clamp<s16>(accel_x, 0, 1024);
  accel_y = MathUtil::Clamp<s16>(accel_y, 0, 1024);
  accel_z = MathUtil::Clamp<s16>(accel_z, 0, 1024);

  nc_data.ax = (accel_x >> 2) & 0xFF;
  nc_data.ay = (accel_y >> 2) & 0xFF;
  nc_data.az = (accel_z >> 2) & 0xFF;
  nc_data.bt.acc_x_lsb = accel_x & 0x3;
  nc_data.bt.acc_y_lsb = accel_y & 0x3;
  nc_data.bt.acc_z_lsb = accel_z & 0x3;

  std::memcpy(data, &nc_data, sizeof(wm_nc));
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
