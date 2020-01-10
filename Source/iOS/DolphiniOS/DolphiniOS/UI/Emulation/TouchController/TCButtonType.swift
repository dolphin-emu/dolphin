// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

import Foundation

// ButtonManager::ButtonType
enum TCButtonType: Int
{
  // GameCube
  case BUTTON_A = 0
  case BUTTON_B = 1
  case BUTTON_START = 2
  case BUTTON_X = 3
  case BUTTON_Y = 4
  case BUTTON_Z = 5
  case BUTTON_UP = 6
  case BUTTON_DOWN = 7
  case BUTTON_LEFT = 8
  case BUTTON_RIGHT = 9
  case STICK_MAIN = 10
  case STICK_C = 15
  case TRIGGER_L = 20
  case TRIGGER_R = 21
  // Wiimote
  case WIIMOTE_BUTTON_A = 100
  case WIIMOTE_BUTTON_B = 101
  case WIIMOTE_BUTTON_MINUS = 102
  case WIIMOTE_BUTTON_PLUS = 103
  case WIIMOTE_BUTTON_HOME = 104
  case WIIMOTE_BUTTON_1 = 105
  case WIIMOTE_BUTTON_2 = 106
  case WIIMOTE_UP = 107
  case WIIMOTE_DOWN = 108
  case WIIMOTE_LEFT = 109
  case WIIMOTE_RIGHT = 110
  case WIIMOTE_IR = 111
  case WIIMOTE_IR_UP = 112
  case WIIMOTE_IR_DOWN = 113
  case WIIMOTE_IR_LEFT = 114
  case WIIMOTE_IR_RIGHT = 115
  case WIIMOTE_IR_FORWARD = 116
  case WIIMOTE_IR_BACKWARD = 117
  case WIIMOTE_IR_HIDE = 118
  case WIIMOTE_SWING = 119
  case WIIMOTE_SWING_UP = 120
  case WIIMOTE_SWING_DOWN = 121
  case WIIMOTE_SWING_LEFT = 122
  case WIIMOTE_SWING_RIGHT = 123
  case WIIMOTE_SWING_FORWARD = 124
  case WIIMOTE_SWING_BACKWARD = 125
  case WIIMOTE_TILT = 126
  case WIIMOTE_TILT_FORWARD = 127
  case WIIMOTE_TILT_BACKWARD = 128
  case WIIMOTE_TILT_LEFT = 129
  case WIIMOTE_TILT_RIGHT = 130
  case WIIMOTE_TILT_MODIFIER = 131
  case WIIMOTE_SHAKE_X = 132
  case WIIMOTE_SHAKE_Y = 133
  case WIIMOTE_SHAKE_Z = 134
  // Nunchuk
  case NUNCHUK_BUTTON_C = 200
  case NUNCHUK_BUTTON_Z = 201
  case NUNCHUK_STICK = 202
  case NUNCHUK_SWING = 207
  case NUNCHUK_SWING_UP = 208
  case NUNCHUK_SWING_DOWN = 209
  case NUNCHUK_SWING_LEFT = 210
  case NUNCHUK_SWING_RIGHT = 211
  case NUNCHUK_SWING_FORWARD = 212
  case NUNCHUK_SWING_BACKWARD = 213
  case NUNCHUK_TILT = 214
  case NUNCHUK_TILT_FORWARD = 215
  case NUNCHUK_TILT_BACKWARD = 216
  case NUNCHUK_TILT_LEFT = 217
  case NUNCHUK_TILT_RIGHT = 218
  case NUNCHUK_TILT_MODIFIER = 219
  case NUNCHUK_SHAKE_X = 220
  case NUNCHUK_SHAKE_Y = 221
  case NUNCHUK_SHAKE_Z = 222
  // Classic Controller
  case CLASSIC_BUTTON_A = 300
  case CLASSIC_BUTTON_B = 301
  case CLASSIC_BUTTON_X = 302
  case CLASSIC_BUTTON_Y = 303
  case CLASSIC_BUTTON_MINUS = 304
  case CLASSIC_BUTTON_PLUS = 305
  case CLASSIC_BUTTON_HOME = 306
  case CLASSIC_BUTTON_ZL = 307
  case CLASSIC_BUTTON_ZR = 308
  case CLASSIC_DPAD_UP = 309
  case CLASSIC_DPAD_DOWN = 310
  case CLASSIC_DPAD_LEFT = 311
  case CLASSIC_DPAD_RIGHT = 312
  case CLASSIC_STICK_LEFT = 313
  case CLASSIC_STICK_LEFT_UP = 314
  case CLASSIC_STICK_LEFT_DOWN = 315
  case CLASSIC_STICK_LEFT_LEFT = 316
  case CLASSIC_STICK_LEFT_RIGHT = 317
  case CLASSIC_STICK_RIGHT = 318
  case CLASSIC_STICK_RIGHT_UP = 319
  case CLASSIC_STICK_RIGHT_DOWN = 320
  case CLASSIC_STICK_RIGHT_LEFT = 321
  case CLASSIC_STICK_RIGHT_RIGHT = 322
  case CLASSIC_TRIGGER_L = 323
  case CLASSIC_TRIGGER_R = 324
  // Wiimote IMU
  case WIIMOTE_ACCEL_LEFT = 625
  case WIIMOTE_ACCEL_RIGHT = 626
  case WIIMOTE_ACCEL_FORWARD = 627
  case WIIMOTE_ACCEL_BACKWARD = 628
  case WIIMOTE_ACCEL_UP = 629
  case WIIMOTE_ACCEL_DOWN = 630
  case WIIMOTE_GYRO_PITCH_UP = 631
  case WIIMOTE_GYRO_PITCH_DOWN = 632
  case WIIMOTE_GYRO_ROLL_LEFT = 633
  case WIIMOTE_GYRO_ROLL_RIGHT = 634
  case WIIMOTE_GYRO_YAW_LEFT = 635
  case WIIMOTE_GYRO_YAW_RIGHT = 636
  // TODO: Classic Controller, Guitar, Drums, Turntable, Rumble
  
  func getImageName() -> String
  {
    switch self
    {
    case .BUTTON_A:
      return "gcpad_a"
    case .BUTTON_B:
      return "gcpad_b"
    case .BUTTON_START:
      return "gcpad_start"
    case .BUTTON_X:
      return "gcpad_x"
    case .BUTTON_Y:
      return "gcpad_y"
    case .BUTTON_Z:
      return "gcpad_z"
    case .BUTTON_UP, .BUTTON_DOWN, .BUTTON_LEFT, .BUTTON_RIGHT, .WIIMOTE_UP, .WIIMOTE_DOWN,
         .WIIMOTE_LEFT, .WIIMOTE_RIGHT, .CLASSIC_DPAD_UP, .CLASSIC_DPAD_DOWN, .CLASSIC_DPAD_LEFT, .CLASSIC_DPAD_RIGHT:
      return "gcwii_dpad"
    case .STICK_MAIN, .WIIMOTE_IR, .NUNCHUK_STICK, .CLASSIC_STICK_LEFT, .CLASSIC_STICK_RIGHT:
      return "gcwii_joystick"
    case .STICK_C:
      return "gcpad_c"
    case .TRIGGER_L:
      return "gcpad_l"
    case .TRIGGER_R:
      return "gcpad_r"
    case .WIIMOTE_BUTTON_A:
      return "wiimote_a"
    case .WIIMOTE_BUTTON_B:
      return "wiimote_b"
    case .WIIMOTE_BUTTON_MINUS, .CLASSIC_BUTTON_MINUS:
      return "wiimote_minus"
    case .WIIMOTE_BUTTON_PLUS, .CLASSIC_BUTTON_PLUS:
      return "wiimote_plus"
    case .WIIMOTE_BUTTON_HOME, .CLASSIC_BUTTON_HOME:
      return "wiimote_home"
    case .WIIMOTE_BUTTON_1:
      return "wiimote_one"
    case .WIIMOTE_BUTTON_2:
      return "wiimote_two"
    case .NUNCHUK_BUTTON_C:
      return "nunchuk_c"
    case .NUNCHUK_BUTTON_Z:
      return "nunchuk_z"
    case .CLASSIC_BUTTON_A:
      return "classic_a"
    case .CLASSIC_BUTTON_B:
      return "classic_b"
    case .CLASSIC_BUTTON_X:
      return "classic_x"
    case .CLASSIC_BUTTON_Y:
      return "classic_y"
    case .CLASSIC_BUTTON_ZL:
      return "classic_zl"
    case .CLASSIC_BUTTON_ZR:
      return "classic_zr"
    case .CLASSIC_TRIGGER_L:
      return "classic_l"
    case .CLASSIC_TRIGGER_R:
      return "classic_r"
    default:
      return "gcpad_a"
    }
  }
  
  func getButtonScale() -> CGFloat
  {
    switch self
    {
    case .BUTTON_A, .BUTTON_Z, .TRIGGER_L, .TRIGGER_R, .WIIMOTE_BUTTON_B, .NUNCHUK_BUTTON_Z, .CLASSIC_BUTTON_ZL, .CLASSIC_BUTTON_ZR, .CLASSIC_TRIGGER_L, .CLASSIC_TRIGGER_R:
      return 0.6
    case .BUTTON_B, .BUTTON_START, .WIIMOTE_BUTTON_1, .WIIMOTE_BUTTON_2, .WIIMOTE_BUTTON_A, .CLASSIC_BUTTON_A, .CLASSIC_BUTTON_B, .CLASSIC_BUTTON_X, .CLASSIC_BUTTON_Y:
      return 0.33
    case .BUTTON_X, .BUTTON_Y:
      return 0.5
    case .WIIMOTE_BUTTON_PLUS, .WIIMOTE_BUTTON_MINUS, .CLASSIC_BUTTON_MINUS, .CLASSIC_BUTTON_PLUS:
      return 0.2
    case .WIIMOTE_BUTTON_HOME, .CLASSIC_BUTTON_HOME:
      return 0.25
    case .NUNCHUK_BUTTON_C:
      return 0.4
    default:
      return 1.0
    }
  }
  
}
