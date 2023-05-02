// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/VR/VRBase.h"

enum class xrButton
{
  A = 0x00000001,  // Set for trigger pulled on the Gear VR and Go Controllers
  B = 0x00000002,
  RThumb = 0x00000004,
  RShoulder = 0x00000008,

  X = 0x00000100,
  Y = 0x00000200,
  LThumb = 0x00000400,
  LShoulder = 0x00000800,

  Up = 0x00010000,
  Down = 0x00020000,
  Left = 0x00040000,
  Right = 0x00080000,
  Enter = 0x00100000,  //< Set for touchpad click on the Go Controller, menu
  // button on Left Quest Controller
  Back = 0x00200000,  //< Back button on the Go Controller (only set when
  // a short press comes up)
  Grip = 0x04000000,    //< grip trigger engaged
  Trigger = 0x20000000  //< Index Trigger engaged
};

void IN_VRInit(engine_t* engine);
void IN_VRInputFrame(engine_t* engine);
uint32_t IN_VRGetButtonState(int controllerIndex);
XrVector2f IN_VRGetJoystickState(int controllerIndex);
XrPosef IN_VRGetPose(int controllerIndex);
void INVR_Vibrate(int duration, int chan, float intensity);
