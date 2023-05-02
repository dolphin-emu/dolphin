// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace ciface::Touch
{
enum ControlID
{
  GCPAD_A_BUTTON = 0,
  GCPAD_B_BUTTON = 1,
  GCPAD_X_BUTTON = 2,
  GCPAD_Y_BUTTON = 3,
  GCPAD_Z_BUTTON = 4,
  GCPAD_START_BUTTON = 5,
  GCPAD_DPAD_UP = 6,
  GCPAD_DPAD_DOWN = 7,
  GCPAD_DPAD_LEFT = 8,
  GCPAD_DPAD_RIGHT = 9,
  GCPAD_L_DIGITAL = 10,
  GCPAD_R_DIGITAL = 11,
  GCPAD_L_ANALOG = 12,
  GCPAD_R_ANALOG = 13,
  GCPAD_MAIN_STICK_X = 14,
  GCPAD_MAIN_STICK_Y = 15,
  GCPAD_C_STICK_X = 16,
  GCPAD_C_STICK_Y = 17,

  WIIMOTE_A_BUTTON = 18,
  WIIMOTE_B_BUTTON = 19,
  WIIMOTE_ONE_BUTTON = 20,
  WIIMOTE_TWO_BUTTON = 21,
  WIIMOTE_PLUS_BUTTON = 22,
  WIIMOTE_MINUS_BUTTON = 23,
  WIIMOTE_HOME_BUTTON = 24,
  WIIMOTE_DPAD_UP = 25,
  WIIMOTE_DPAD_DOWN = 26,
  WIIMOTE_DPAD_LEFT = 27,
  WIIMOTE_DPAD_RIGHT = 28,
  WIIMOTE_IR_X = 29,
  WIIMOTE_IR_Y = 30,

  NUNCHUK_C_BUTTON = 31,
  NUNCHUK_Z_BUTTON = 32,
  NUNCHUK_STICK_X = 33,
  NUNCHUK_STICK_Y = 34,

  CLASSIC_A_BUTTON = 35,
  CLASSIC_B_BUTTON = 36,
  CLASSIC_X_BUTTON = 37,
  CLASSIC_Y_BUTTON = 38,
  CLASSIC_ZL_BUTTON = 39,
  CLASSIC_ZR_BUTTON = 40,
  CLASSIC_PLUS_BUTTON = 41,
  CLASSIC_MINUS_BUTTON = 42,
  CLASSIC_HOME_BUTTON = 43,
  CLASSIC_DPAD_UP = 44,
  CLASSIC_DPAD_DOWN = 45,
  CLASSIC_DPAD_LEFT = 46,
  CLASSIC_DPAD_RIGHT = 47,
  CLASSIC_L_DIGITAL = 48,
  CLASSIC_R_DIGITAL = 49,
  CLASSIC_L_ANALOG = 50,
  CLASSIC_R_ANALOG = 51,
  CLASSIC_LEFT_STICK_X = 52,
  CLASSIC_LEFT_STICK_Y = 53,
  CLASSIC_RIGHT_STICK_X = 54,
  CLASSIC_RIGHT_STICK_Y = 55,

  NUMBER_OF_CONTROLS,

  FIRST_GC_CONTROL = GCPAD_A_BUTTON,
  LAST_GC_CONTROL = GCPAD_C_STICK_Y,
  FIRST_WII_CONTROL = WIIMOTE_A_BUTTON,
  LAST_WII_CONTROL = CLASSIC_RIGHT_STICK_Y,

};
void RegisterGameCubeInputOverrider(int controller_index);
void RegisterWiiInputOverrider(int controller_index);
void UnregisterGameCubeInputOverrider(int controller_index);
void UnregisterWiiInputOverrider(int controller_index);

void SetControlState(int controller_index, ControlID control, double state);
void ClearControlState(int controller_index, ControlID control);

// Angle is in radians and should be non-negative
double GetGateRadiusAtAngle(int controller_index, ControlID stick, double angle);
}  // namespace ciface::Touch
