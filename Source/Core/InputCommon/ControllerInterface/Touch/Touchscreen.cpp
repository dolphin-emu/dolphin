// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Touch/Touchscreen.h"

#include <sstream>
#include <thread>
#ifdef ANDROID
#include <jni/AndroidCommon/IDCache.h>
#endif

#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::Touch
{
// Touchscreens and stuff
std::string Touchscreen::GetName() const
{
  return "Touchscreen";
}

std::string Touchscreen::GetSource() const
{
  return "Android";
}

Touchscreen::Touchscreen(int pad_id, bool accelerometer_enabled, bool gyroscope_enabled)
    : m_pad_id(pad_id)
{
  // GC
  AddInput(new Button(m_pad_id, ButtonManager::BUTTON_A));
  AddInput(new Button(m_pad_id, ButtonManager::BUTTON_B));
  AddInput(new Button(m_pad_id, ButtonManager::BUTTON_START));
  AddInput(new Button(m_pad_id, ButtonManager::BUTTON_X));
  AddInput(new Button(m_pad_id, ButtonManager::BUTTON_Y));
  AddInput(new Button(m_pad_id, ButtonManager::BUTTON_Z));
  AddInput(new Button(m_pad_id, ButtonManager::BUTTON_UP));
  AddInput(new Button(m_pad_id, ButtonManager::BUTTON_DOWN));
  AddInput(new Button(m_pad_id, ButtonManager::BUTTON_LEFT));
  AddInput(new Button(m_pad_id, ButtonManager::BUTTON_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::STICK_MAIN_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::STICK_MAIN_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::STICK_MAIN_UP));
  AddInput(new Axis(m_pad_id, ButtonManager::STICK_MAIN_DOWN));
  AddInput(new Axis(m_pad_id, ButtonManager::STICK_C_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::STICK_C_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::STICK_C_UP));
  AddInput(new Axis(m_pad_id, ButtonManager::STICK_C_DOWN));
  AddInput(new Axis(m_pad_id, ButtonManager::TRIGGER_L));
  AddInput(new Axis(m_pad_id, ButtonManager::TRIGGER_R));

  // Wiimote
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_BUTTON_A));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_BUTTON_B));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_BUTTON_MINUS));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_BUTTON_PLUS));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_BUTTON_HOME));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_BUTTON_1));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_BUTTON_2));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_UP));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_DOWN));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_LEFT));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_RIGHT));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_IR_HIDE));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_TILT_MODIFIER));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_SHAKE_X));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_SHAKE_Y));
  AddInput(new Button(m_pad_id, ButtonManager::WIIMOTE_SHAKE_Z));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_IR_UP));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_IR_DOWN));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_IR_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_IR_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_IR_FORWARD));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_IR_BACKWARD));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_SWING_UP));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_SWING_DOWN));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_SWING_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_SWING_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_SWING_FORWARD));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_SWING_BACKWARD));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_TILT_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_TILT_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_TILT_FORWARD));
  AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_TILT_BACKWARD));

  // Wii ext: Nunchuk
  AddInput(new Button(m_pad_id, ButtonManager::NUNCHUK_BUTTON_C));
  AddInput(new Button(m_pad_id, ButtonManager::NUNCHUK_BUTTON_Z));
  AddInput(new Button(m_pad_id, ButtonManager::NUNCHUK_TILT_MODIFIER));
  AddInput(new Button(m_pad_id, ButtonManager::NUNCHUK_SHAKE_X));
  AddInput(new Button(m_pad_id, ButtonManager::NUNCHUK_SHAKE_Y));
  AddInput(new Button(m_pad_id, ButtonManager::NUNCHUK_SHAKE_Z));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_STICK_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_STICK_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_STICK_UP));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_STICK_DOWN));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_SWING_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_SWING_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_SWING_UP));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_SWING_DOWN));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_SWING_FORWARD));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_SWING_BACKWARD));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_TILT_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_TILT_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_TILT_FORWARD));
  AddInput(new Axis(m_pad_id, ButtonManager::NUNCHUK_TILT_BACKWARD));

  // Wii ext: Classic
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_BUTTON_A));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_BUTTON_B));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_BUTTON_X));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_BUTTON_Y));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_BUTTON_MINUS));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_BUTTON_PLUS));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_BUTTON_HOME));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_BUTTON_ZL));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_BUTTON_ZR));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_DPAD_UP));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_DPAD_DOWN));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_DPAD_LEFT));
  AddInput(new Button(m_pad_id, ButtonManager::CLASSIC_DPAD_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::CLASSIC_STICK_LEFT_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::CLASSIC_STICK_LEFT_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::CLASSIC_STICK_LEFT_UP));
  AddInput(new Axis(m_pad_id, ButtonManager::CLASSIC_STICK_LEFT_DOWN));
  AddInput(new Axis(m_pad_id, ButtonManager::CLASSIC_STICK_RIGHT_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::CLASSIC_STICK_RIGHT_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::CLASSIC_STICK_RIGHT_UP));
  AddInput(new Axis(m_pad_id, ButtonManager::CLASSIC_STICK_RIGHT_DOWN));
  AddInput(new Axis(m_pad_id, ButtonManager::CLASSIC_TRIGGER_L));
  AddInput(new Axis(m_pad_id, ButtonManager::CLASSIC_TRIGGER_R));

  // Wii-ext: Guitar
  AddInput(new Button(m_pad_id, ButtonManager::GUITAR_BUTTON_MINUS));
  AddInput(new Button(m_pad_id, ButtonManager::GUITAR_BUTTON_PLUS));
  AddInput(new Button(m_pad_id, ButtonManager::GUITAR_FRET_GREEN));
  AddInput(new Button(m_pad_id, ButtonManager::GUITAR_FRET_RED));
  AddInput(new Button(m_pad_id, ButtonManager::GUITAR_FRET_YELLOW));
  AddInput(new Button(m_pad_id, ButtonManager::GUITAR_FRET_BLUE));
  AddInput(new Button(m_pad_id, ButtonManager::GUITAR_FRET_ORANGE));
  AddInput(new Button(m_pad_id, ButtonManager::GUITAR_STRUM_UP));
  AddInput(new Button(m_pad_id, ButtonManager::GUITAR_STRUM_DOWN));
  AddInput(new Axis(m_pad_id, ButtonManager::GUITAR_STICK_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::GUITAR_STICK_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::GUITAR_STICK_UP));
  AddInput(new Axis(m_pad_id, ButtonManager::GUITAR_STICK_DOWN));
  AddInput(new Axis(m_pad_id, ButtonManager::GUITAR_WHAMMY_BAR));

  // Wii-ext: Drums
  AddInput(new Button(m_pad_id, ButtonManager::DRUMS_BUTTON_MINUS));
  AddInput(new Button(m_pad_id, ButtonManager::DRUMS_BUTTON_PLUS));
  AddInput(new Button(m_pad_id, ButtonManager::DRUMS_PAD_RED));
  AddInput(new Button(m_pad_id, ButtonManager::DRUMS_PAD_YELLOW));
  AddInput(new Button(m_pad_id, ButtonManager::DRUMS_PAD_BLUE));
  AddInput(new Button(m_pad_id, ButtonManager::DRUMS_PAD_GREEN));
  AddInput(new Button(m_pad_id, ButtonManager::DRUMS_PAD_ORANGE));
  AddInput(new Button(m_pad_id, ButtonManager::DRUMS_PAD_BASS));
  AddInput(new Axis(m_pad_id, ButtonManager::DRUMS_STICK_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::DRUMS_STICK_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::DRUMS_STICK_UP));
  AddInput(new Axis(m_pad_id, ButtonManager::DRUMS_STICK_DOWN));

  // Wii-ext: Turntable
  AddInput(new Button(m_pad_id, ButtonManager::TURNTABLE_BUTTON_GREEN_LEFT));
  AddInput(new Button(m_pad_id, ButtonManager::TURNTABLE_BUTTON_RED_LEFT));
  AddInput(new Button(m_pad_id, ButtonManager::TURNTABLE_BUTTON_BLUE_LEFT));
  AddInput(new Button(m_pad_id, ButtonManager::TURNTABLE_BUTTON_GREEN_RIGHT));
  AddInput(new Button(m_pad_id, ButtonManager::TURNTABLE_BUTTON_RED_RIGHT));
  AddInput(new Button(m_pad_id, ButtonManager::TURNTABLE_BUTTON_BLUE_RIGHT));
  AddInput(new Button(m_pad_id, ButtonManager::TURNTABLE_BUTTON_MINUS));
  AddInput(new Button(m_pad_id, ButtonManager::TURNTABLE_BUTTON_PLUS));
  AddInput(new Button(m_pad_id, ButtonManager::TURNTABLE_BUTTON_HOME));
  AddInput(new Button(m_pad_id, ButtonManager::TURNTABLE_BUTTON_EUPHORIA));
  AddInput(new Axis(m_pad_id, ButtonManager::TURNTABLE_TABLE_LEFT_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::TURNTABLE_TABLE_LEFT_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::TURNTABLE_TABLE_RIGHT_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::TURNTABLE_TABLE_RIGHT_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::TURNTABLE_STICK_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::TURNTABLE_STICK_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::TURNTABLE_STICK_UP));
  AddInput(new Axis(m_pad_id, ButtonManager::TURNTABLE_STICK_DOWN));
  AddInput(new Axis(m_pad_id, ButtonManager::TURNTABLE_CROSSFADE_LEFT));
  AddInput(new Axis(m_pad_id, ButtonManager::TURNTABLE_CROSSFADE_RIGHT));
  AddInput(new Axis(m_pad_id, ButtonManager::TURNTABLE_EFFECT_DIAL));

  // Wiimote IMU
  // Only add inputs if we actually can receive data from the relevant sensor.
  // Whether inputs exist affects what WiimoteEmu gets when calling ControlReference::BoundCount.
  if (accelerometer_enabled)
  {
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_ACCEL_LEFT));
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_ACCEL_RIGHT));
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_ACCEL_FORWARD));
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_ACCEL_BACKWARD));
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_ACCEL_UP));
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_ACCEL_DOWN));
  }
  if (gyroscope_enabled)
  {
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_GYRO_PITCH_UP));
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_GYRO_PITCH_DOWN));
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_GYRO_ROLL_LEFT));
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_GYRO_ROLL_RIGHT));
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_GYRO_YAW_LEFT));
    AddInput(new Axis(m_pad_id, ButtonManager::WIIMOTE_GYRO_YAW_RIGHT));
  }

  // Rumble
  AddOutput(new Motor(m_pad_id, ButtonManager::RUMBLE));
}
// Buttons and stuff

std::string Touchscreen::Button::GetName() const
{
  std::ostringstream ss;
  ss << "Button " << static_cast<int>(m_index);
  return ss.str();
}

ControlState Touchscreen::Button::GetState() const
{
  return ButtonManager::GetButtonPressed(m_pad_id, m_index);
}

std::string Touchscreen::Axis::GetName() const
{
  std::ostringstream ss;
  ss << "Axis " << static_cast<int>(m_index);
  return ss.str();
}

ControlState Touchscreen::Axis::GetState() const
{
  return ButtonManager::GetAxisValue(m_pad_id, m_index) * m_neg;
}

Touchscreen::Motor::~Motor()
{
}

std::string Touchscreen::Motor::GetName() const
{
  std::ostringstream ss;
  ss << "Rumble " << static_cast<int>(m_index);
  return ss.str();
}

void Touchscreen::Motor::SetState(ControlState state)
{
  if (state > 0)
  {
    std::thread(Rumble, m_pad_id, state).detach();
  }
}

void Touchscreen::Motor::Rumble(int pad_id, double state)
{
#ifdef ANDROID
  JNIEnv* env = IDCache::GetEnvForThread();
  env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(), IDCache::GetDoRumble(), pad_id,
                            state);
#endif
}
}  // namespace ciface::Touch
