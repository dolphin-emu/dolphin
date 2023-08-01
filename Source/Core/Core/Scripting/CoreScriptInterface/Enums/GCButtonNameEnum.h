#pragma once

namespace Scripting
{

#ifdef __cplusplus
extern "C" {
#endif

// GCButtonNameEnum is used to represent the names of buttons on the GameCube controller
// for various APIs that take a GameCube button as input or return a GameCube button's state.

// Each of these enums corresponds to a similarly-named variable in the Movie::ControllerState
// struct
enum GCButtonNameEnum
{
  A = 0,
  B = 1,
  X = 2,
  Y = 3,
  Z = 4,
  L = 5,  // Refers to the digital L button (either pressed or not pressed)
  R = 6,  // Refers to the digital R button (either pressed or not pressed)
  DPadUp = 7,
  DPadDown = 8,
  DPadLeft = 9,
  DPadRight = 10,
  AnalogStickX = 11,
  AnalogStickY = 12,
  CStickX = 13,
  CStickY = 14,
  TriggerL = 15,  // Refers to the analog L trigger value (between 0-255)
  TriggerR = 16,  // Refers to the analog R trigger value (between 0-255)
  Start = 17,
  Reset = 18,
  Disc = 19,
  GetOrigin = 20,
  IsConnected = 21,
  UnknownButton = 22
};

#ifdef __cplusplus
}
#endif

}  // namespace Scripting
