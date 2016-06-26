// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/Android/Android.h"

#include <memory>
#include <sstream>
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface
{
namespace Android
{
void Init()
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

int Touchscreen::GetId() const
{
  return _padID;
}
Touchscreen::Touchscreen(int padID) : _padID(padID)
{
  // GC
  AddInput(std::make_unique<Button>(_padID, ButtonManager::BUTTON_A));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::BUTTON_B));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::BUTTON_START));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::BUTTON_X));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::BUTTON_Y));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::BUTTON_Z));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::BUTTON_UP));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::BUTTON_DOWN));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::BUTTON_LEFT));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::BUTTON_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::STICK_MAIN_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::STICK_MAIN_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::STICK_MAIN_UP),
                  std::make_unique<Axis>(_padID, ButtonManager::STICK_MAIN_DOWN));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::STICK_C_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::STICK_C_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::STICK_C_UP),
                  std::make_unique<Axis>(_padID, ButtonManager::STICK_C_DOWN));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::TRIGGER_L),
                  std::make_unique<Axis>(_padID, ButtonManager::TRIGGER_L));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::TRIGGER_R),
                  std::make_unique<Axis>(_padID, ButtonManager::TRIGGER_R));

  // Wiimote
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_BUTTON_A));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_BUTTON_B));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_BUTTON_MINUS));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_BUTTON_PLUS));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_BUTTON_HOME));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_BUTTON_1));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_BUTTON_2));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_UP));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_DOWN));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_LEFT));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_RIGHT));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_IR_HIDE));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_TILT_MODIFIER));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_SHAKE_X));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_SHAKE_Y));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::WIIMOTE_SHAKE_Z));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_IR_UP),
                  std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_IR_DOWN));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_IR_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_IR_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_IR_FORWARD),
                  std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_IR_BACKWARD));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_SWING_UP),
                  std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_SWING_DOWN));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_SWING_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_SWING_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_SWING_FORWARD),
                  std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_SWING_BACKWARD));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_TILT_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_TILT_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_TILT_FORWARD),
                  std::make_unique<Axis>(_padID, ButtonManager::WIIMOTE_TILT_BACKWARD));

  // Wii ext: Nunchuk
  AddInput(std::make_unique<Button>(_padID, ButtonManager::NUNCHUK_BUTTON_C));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::NUNCHUK_BUTTON_Z));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::NUNCHUK_TILT_MODIFIER));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::NUNCHUK_SHAKE_X));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::NUNCHUK_SHAKE_Y));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::NUNCHUK_SHAKE_Z));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_STICK_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_STICK_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_STICK_UP),
                  std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_STICK_DOWN));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_SWING_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_SWING_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_SWING_UP),
                  std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_SWING_DOWN));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_SWING_FORWARD),
                  std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_SWING_BACKWARD));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_TILT_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_TILT_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_TILT_FORWARD),
                  std::make_unique<Axis>(_padID, ButtonManager::NUNCHUK_TILT_BACKWARD));

  // Wii ext: Classic
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_BUTTON_A));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_BUTTON_B));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_BUTTON_X));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_BUTTON_Y));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_BUTTON_MINUS));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_BUTTON_PLUS));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_BUTTON_HOME));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_BUTTON_ZL));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_BUTTON_ZR));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_DPAD_UP));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_DPAD_DOWN));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_DPAD_LEFT));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::CLASSIC_DPAD_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_STICK_LEFT_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_STICK_LEFT_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_STICK_LEFT_UP),
                  std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_STICK_LEFT_DOWN));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_STICK_RIGHT_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_STICK_RIGHT_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_STICK_RIGHT_UP),
                  std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_STICK_RIGHT_DOWN));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_TRIGGER_L),
                  std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_TRIGGER_L));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_TRIGGER_R),
                  std::make_unique<Axis>(_padID, ButtonManager::CLASSIC_TRIGGER_R));

  // Wii-ext: Guitar
  AddInput(std::make_unique<Button>(_padID, ButtonManager::GUITAR_BUTTON_MINUS));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::GUITAR_BUTTON_PLUS));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::GUITAR_FRET_GREEN));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::GUITAR_FRET_RED));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::GUITAR_FRET_YELLOW));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::GUITAR_FRET_BLUE));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::GUITAR_FRET_ORANGE));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::GUITAR_STRUM_UP));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::GUITAR_STRUM_DOWN));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::GUITAR_STICK_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::GUITAR_STICK_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::GUITAR_STICK_UP),
                  std::make_unique<Axis>(_padID, ButtonManager::GUITAR_STICK_DOWN));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::GUITAR_WHAMMY_BAR),
                  std::make_unique<Axis>(_padID, ButtonManager::GUITAR_WHAMMY_BAR));

  // Wii-ext: Drums
  AddInput(std::make_unique<Button>(_padID, ButtonManager::DRUMS_BUTTON_MINUS));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::DRUMS_BUTTON_PLUS));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::DRUMS_PAD_RED));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::DRUMS_PAD_YELLOW));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::DRUMS_PAD_BLUE));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::DRUMS_PAD_GREEN));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::DRUMS_PAD_ORANGE));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::DRUMS_PAD_BASS));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::DRUMS_STICK_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::DRUMS_STICK_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::DRUMS_STICK_UP),
                  std::make_unique<Axis>(_padID, ButtonManager::DRUMS_STICK_DOWN));

  // Wii-ext: Turntable
  AddInput(std::make_unique<Button>(_padID, ButtonManager::TURNTABLE_BUTTON_GREEN_LEFT));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::TURNTABLE_BUTTON_RED_LEFT));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::TURNTABLE_BUTTON_BLUE_LEFT));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::TURNTABLE_BUTTON_GREEN_RIGHT));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::TURNTABLE_BUTTON_RED_RIGHT));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::TURNTABLE_BUTTON_BLUE_RIGHT));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::TURNTABLE_BUTTON_MINUS));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::TURNTABLE_BUTTON_PLUS));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::TURNTABLE_BUTTON_HOME));
  AddInput(std::make_unique<Button>(_padID, ButtonManager::TURNTABLE_BUTTON_EUPHORIA));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_TABLE_LEFT_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_TABLE_LEFT_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_TABLE_RIGHT_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_TABLE_RIGHT_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_STICK_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_STICK_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_STICK_UP),
                  std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_STICK_DOWN));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_CROSSFADE_LEFT),
                  std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_CROSSFADE_RIGHT));
  AddAnalogInputs(std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_EFFECT_DIAL),
                  std::make_unique<Axis>(_padID, ButtonManager::TURNTABLE_EFFECT_DIAL));
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
}
}
