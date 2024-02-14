// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace ciface::VR
{
enum class Button
{
  A = 0x00000001,  // Set for trigger pulled on the Gear VR and Go Controllers
  B = 0x00000002,
  RThumb = 0x00000004,

  X = 0x00000100,
  Y = 0x00000200,
  LThumb = 0x00000400,

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

void RegisterInputOverrider(int controller_index);
void UnregisterInputOverrider(int controller_index);

void UpdateInput(int id, int l, int r, float x, float y, float jlx, float jly, float jrx,
                 float jry);
}  // namespace ciface::VR
