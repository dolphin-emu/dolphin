#pragma once

#include "VRBase.h"

typedef enum xrButton_
{
  xrButton_A = 0x00000001,  // Set for trigger pulled on the Gear VR and Go Controllers
  xrButton_B = 0x00000002,
  xrButton_RThumb = 0x00000004,
  xrButton_RShoulder = 0x00000008,

  xrButton_X = 0x00000100,
  xrButton_Y = 0x00000200,
  xrButton_LThumb = 0x00000400,
  xrButton_LShoulder = 0x00000800,

  xrButton_Up = 0x00010000,
  xrButton_Down = 0x00020000,
  xrButton_Left = 0x00040000,
  xrButton_Right = 0x00080000,
  xrButton_Enter = 0x00100000,  //< Set for touchpad click on the Go Controller, menu
  // button on Left Quest Controller
  xrButton_Back = 0x00200000,  //< Back button on the Go Controller (only set when
  // a short press comes up)
  xrButton_GripTrigger = 0x04000000,  //< grip trigger engaged
  xrButton_Trigger = 0x20000000,      //< Index Trigger engaged
  xrButton_Joystick = 0x80000000,     //< Click of the Joystick

  xrButton_EnumSize = 0x7fffffff
} xrButton;

void IN_VRInit(engine_t* engine);
void IN_VRInputFrame(engine_t* engine);
uint32_t IN_VRGetButtonState(int controllerIndex);
XrVector2f IN_VRGetJoystickState(int controllerIndex);
XrPosef IN_VRGetPose(int controllerIndex);
void INVR_Vibrate(int duration, int chan, float intensity);
