// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/Android/Android.h"
#include <jni/AndroidCommon/IDCache.h>
#include <sstream>
#include <thread>
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface
{
namespace Android
{
void PopulateDevices()
{
  for (int i = 0; i < 8; ++i)
    g_controller_interface.AddDevice(std::make_shared<Touchscreen>(i));
}

// Touchscreens and stuff
std::string Touchscreen::GetName() const
{
  return "Touchscreen";
}

std::string Touchscreen::GetSource() const
{
  return "Android";
}

Touchscreen::Touchscreen(int padID) : _padID(padID)
{
  // GC
  AddInput(new Button(_padID, ButtonManager::BUTTON_A));
  AddInput(new Button(_padID, ButtonManager::BUTTON_B));
  AddInput(new Button(_padID, ButtonManager::BUTTON_START));
  AddInput(new Button(_padID, ButtonManager::BUTTON_X));
  AddInput(new Button(_padID, ButtonManager::BUTTON_Y));
  AddInput(new Button(_padID, ButtonManager::BUTTON_Z));
  AddInput(new Button(_padID, ButtonManager::BUTTON_UP));
  AddInput(new Button(_padID, ButtonManager::BUTTON_DOWN));
  AddInput(new Button(_padID, ButtonManager::BUTTON_LEFT));
  AddInput(new Button(_padID, ButtonManager::BUTTON_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::STICK_MAIN_LEFT));
  AddInput(new Axis(_padID, ButtonManager::STICK_MAIN_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::STICK_MAIN_UP));
  AddInput(new Axis(_padID, ButtonManager::STICK_MAIN_DOWN));
  AddInput(new Axis(_padID, ButtonManager::STICK_C_LEFT));
  AddInput(new Axis(_padID, ButtonManager::STICK_C_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::STICK_C_UP));
  AddInput(new Axis(_padID, ButtonManager::STICK_C_DOWN));
  AddInput(new Axis(_padID, ButtonManager::TRIGGER_L));
  AddInput(new Axis(_padID, ButtonManager::TRIGGER_R));

  // Wiimote
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_BUTTON_A));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_BUTTON_B));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_BUTTON_MINUS));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_BUTTON_PLUS));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_BUTTON_HOME));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_BUTTON_1));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_BUTTON_2));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_UP));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_DOWN));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_LEFT));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_RIGHT));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_IR_HIDE));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_TILT_MODIFIER));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_SHAKE_X));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_SHAKE_Y));
  AddInput(new Button(_padID, ButtonManager::WIIMOTE_SHAKE_Z));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_IR_UP));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_IR_DOWN));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_IR_LEFT));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_IR_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_IR_FORWARD));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_IR_BACKWARD));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_SWING_UP));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_SWING_DOWN));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_SWING_LEFT));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_SWING_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_SWING_FORWARD));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_SWING_BACKWARD));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_TILT_LEFT));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_TILT_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_TILT_FORWARD));
  AddInput(new Axis(_padID, ButtonManager::WIIMOTE_TILT_BACKWARD));

  // Wii ext: Nunchuk
  AddInput(new Button(_padID, ButtonManager::NUNCHUK_BUTTON_C));
  AddInput(new Button(_padID, ButtonManager::NUNCHUK_BUTTON_Z));
  AddInput(new Button(_padID, ButtonManager::NUNCHUK_TILT_MODIFIER));
  AddInput(new Button(_padID, ButtonManager::NUNCHUK_SHAKE_X));
  AddInput(new Button(_padID, ButtonManager::NUNCHUK_SHAKE_Y));
  AddInput(new Button(_padID, ButtonManager::NUNCHUK_SHAKE_Z));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_STICK_LEFT));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_STICK_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_STICK_UP));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_STICK_DOWN));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_SWING_LEFT));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_SWING_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_SWING_UP));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_SWING_DOWN));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_SWING_FORWARD));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_SWING_BACKWARD));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_TILT_LEFT));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_TILT_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_TILT_FORWARD));
  AddInput(new Axis(_padID, ButtonManager::NUNCHUK_TILT_BACKWARD));

  // Wii ext: Classic
  AddInput(new Button(_padID, ButtonManager::CLASSIC_BUTTON_A));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_BUTTON_B));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_BUTTON_X));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_BUTTON_Y));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_BUTTON_MINUS));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_BUTTON_PLUS));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_BUTTON_HOME));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_BUTTON_ZL));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_BUTTON_ZR));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_DPAD_UP));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_DPAD_DOWN));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_DPAD_LEFT));
  AddInput(new Button(_padID, ButtonManager::CLASSIC_DPAD_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::CLASSIC_STICK_LEFT_LEFT));
  AddInput(new Axis(_padID, ButtonManager::CLASSIC_STICK_LEFT_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::CLASSIC_STICK_LEFT_UP));
  AddInput(new Axis(_padID, ButtonManager::CLASSIC_STICK_LEFT_DOWN));
  AddInput(new Axis(_padID, ButtonManager::CLASSIC_STICK_RIGHT_LEFT));
  AddInput(new Axis(_padID, ButtonManager::CLASSIC_STICK_RIGHT_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::CLASSIC_STICK_RIGHT_UP));
  AddInput(new Axis(_padID, ButtonManager::CLASSIC_STICK_RIGHT_DOWN));
  AddInput(new Axis(_padID, ButtonManager::CLASSIC_TRIGGER_L));
  AddInput(new Axis(_padID, ButtonManager::CLASSIC_TRIGGER_R));

  // Wii-ext: Guitar
  AddInput(new Button(_padID, ButtonManager::GUITAR_BUTTON_MINUS));
  AddInput(new Button(_padID, ButtonManager::GUITAR_BUTTON_PLUS));
  AddInput(new Button(_padID, ButtonManager::GUITAR_FRET_GREEN));
  AddInput(new Button(_padID, ButtonManager::GUITAR_FRET_RED));
  AddInput(new Button(_padID, ButtonManager::GUITAR_FRET_YELLOW));
  AddInput(new Button(_padID, ButtonManager::GUITAR_FRET_BLUE));
  AddInput(new Button(_padID, ButtonManager::GUITAR_FRET_ORANGE));
  AddInput(new Button(_padID, ButtonManager::GUITAR_STRUM_UP));
  AddInput(new Button(_padID, ButtonManager::GUITAR_STRUM_DOWN));
  AddInput(new Axis(_padID, ButtonManager::GUITAR_STICK_LEFT));
  AddInput(new Axis(_padID, ButtonManager::GUITAR_STICK_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::GUITAR_STICK_UP));
  AddInput(new Axis(_padID, ButtonManager::GUITAR_STICK_DOWN));
  AddInput(new Axis(_padID, ButtonManager::GUITAR_WHAMMY_BAR));

  // Wii-ext: Drums
  AddInput(new Button(_padID, ButtonManager::DRUMS_BUTTON_MINUS));
  AddInput(new Button(_padID, ButtonManager::DRUMS_BUTTON_PLUS));
  AddInput(new Button(_padID, ButtonManager::DRUMS_PAD_RED));
  AddInput(new Button(_padID, ButtonManager::DRUMS_PAD_YELLOW));
  AddInput(new Button(_padID, ButtonManager::DRUMS_PAD_BLUE));
  AddInput(new Button(_padID, ButtonManager::DRUMS_PAD_GREEN));
  AddInput(new Button(_padID, ButtonManager::DRUMS_PAD_ORANGE));
  AddInput(new Button(_padID, ButtonManager::DRUMS_PAD_BASS));
  AddInput(new Axis(_padID, ButtonManager::DRUMS_STICK_LEFT));
  AddInput(new Axis(_padID, ButtonManager::DRUMS_STICK_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::DRUMS_STICK_UP));
  AddInput(new Axis(_padID, ButtonManager::DRUMS_STICK_DOWN));

  // Wii-ext: Turntable
  AddInput(new Button(_padID, ButtonManager::TURNTABLE_BUTTON_GREEN_LEFT));
  AddInput(new Button(_padID, ButtonManager::TURNTABLE_BUTTON_RED_LEFT));
  AddInput(new Button(_padID, ButtonManager::TURNTABLE_BUTTON_BLUE_LEFT));
  AddInput(new Button(_padID, ButtonManager::TURNTABLE_BUTTON_GREEN_RIGHT));
  AddInput(new Button(_padID, ButtonManager::TURNTABLE_BUTTON_RED_RIGHT));
  AddInput(new Button(_padID, ButtonManager::TURNTABLE_BUTTON_BLUE_RIGHT));
  AddInput(new Button(_padID, ButtonManager::TURNTABLE_BUTTON_MINUS));
  AddInput(new Button(_padID, ButtonManager::TURNTABLE_BUTTON_PLUS));
  AddInput(new Button(_padID, ButtonManager::TURNTABLE_BUTTON_HOME));
  AddInput(new Button(_padID, ButtonManager::TURNTABLE_BUTTON_EUPHORIA));
  AddInput(new Axis(_padID, ButtonManager::TURNTABLE_TABLE_LEFT_LEFT));
  AddInput(new Axis(_padID, ButtonManager::TURNTABLE_TABLE_LEFT_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::TURNTABLE_TABLE_RIGHT_LEFT));
  AddInput(new Axis(_padID, ButtonManager::TURNTABLE_TABLE_RIGHT_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::TURNTABLE_STICK_LEFT));
  AddInput(new Axis(_padID, ButtonManager::TURNTABLE_STICK_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::TURNTABLE_STICK_UP));
  AddInput(new Axis(_padID, ButtonManager::TURNTABLE_STICK_DOWN));
  AddInput(new Axis(_padID, ButtonManager::TURNTABLE_CROSSFADE_LEFT));
  AddInput(new Axis(_padID, ButtonManager::TURNTABLE_CROSSFADE_RIGHT));
  AddInput(new Axis(_padID, ButtonManager::TURNTABLE_EFFECT_DIAL));

  // Rumble
  AddOutput(new Motor(_padID, ButtonManager::RUMBLE));
}
// Buttons and stuff

std::string Touchscreen::Button::GetName() const
{
  std::ostringstream ss;
  ss << "Button " << (int)_index;
  return ss.str();
}

ControlState Touchscreen::Button::GetState() const
{
  return ButtonManager::GetButtonPressed(_padID, _index);
}

std::string Touchscreen::Axis::GetName() const
{
  std::ostringstream ss;
  ss << "Axis " << (int)_index;
  return ss.str();
}

ControlState Touchscreen::Axis::GetState() const
{
  return ButtonManager::GetAxisValue(_padID, _index) * _neg;
}

Touchscreen::Motor::~Motor()
{
}

std::string Touchscreen::Motor::GetName() const
{
  std::ostringstream ss;
  ss << "Rumble " << (int)_index;
  return ss.str();
}

void Touchscreen::Motor::SetState(ControlState state)
{
  if (state > 0)
  {
    std::thread(Rumble, _padID, state).detach();
  }
}

void Touchscreen::Motor::Rumble(int padID, double state)
{
  JNIEnv* env;
  IDCache::GetJavaVM()->AttachCurrentThread(&env, nullptr);
  env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(), IDCache::GetDoRumble(), padID, state);
  IDCache::GetJavaVM()->DetachCurrentThread();
}
}  // namespace Android
}  // namespace ciface
