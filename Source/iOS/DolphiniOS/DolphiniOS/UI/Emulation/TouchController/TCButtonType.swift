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
  // TODO: Wiimote, Nunchuk, Classic Controller, Guitar, Drums,
  // Turntable, Rumble
  
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
    case .BUTTON_UP, .BUTTON_DOWN, .BUTTON_LEFT, .BUTTON_RIGHT:
      return "gcwii_dpad"
    case .STICK_MAIN:
      return "gcwii_joystick"
    case .STICK_C:
      return "gcpad_c"
    case .TRIGGER_L:
      return "gcpad_l"
    case .TRIGGER_R:
      return "gcpad_r"
    default:
      return "gcpad_a"
    }
  }
  
  func getButtonScale() -> CGFloat
  {
    switch self
    {
    case .BUTTON_A, .BUTTON_Z, .TRIGGER_L, .TRIGGER_R:
      return 0.6
    case .BUTTON_B, .BUTTON_START:
      return 0.33
    case .BUTTON_X, .BUTTON_Y:
      return 0.5
    default:
      return 1.0
    }
  }
  
}
