// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/VR/DolphinVR.h"

#include "InputCommon/ControllerInterface/Touch/InputOverrider.h"
#include "InputCommon/ControllerInterface/VR/OpenXR.h"

using Common::VR::Button;

namespace ciface::VR
{
void RegisterInputOverrider(int controller_index)
{
  Touch::RegisterGameCubeInputOverrider(controller_index);
  Touch::RegisterWiiInputOverrider(controller_index);
}

void UnregisterInputOverrider(int controller_index)
{
  Touch::UnregisterGameCubeInputOverrider(controller_index);
  Touch::UnregisterWiiInputOverrider(controller_index);
}

void UpdateInput(int id, int l, int r, float x, float y, float jlx, float jly, float jrx, float jry)
{
  // Left controller to GCPad
  SetControlState(id, Touch::GCPAD_X_BUTTON, l & (int)Button::Trigger ? 1 : 0);
  SetControlState(id, Touch::GCPAD_Y_BUTTON, l & (int)Button::Grip ? 1 : 0);
  SetControlState(id, Touch::GCPAD_L_ANALOG, l & (int)Button::X ? 1 : 0);
  SetControlState(id, Touch::GCPAD_R_ANALOG, l & (int)Button::Y ? 1 : 0);
  SetControlState(id, Touch::GCPAD_L_DIGITAL, l & (int)Button::X ? 1 : 0);
  SetControlState(id, Touch::GCPAD_R_DIGITAL, l & (int)Button::Y ? 1 : 0);
  SetControlState(id, Touch::GCPAD_C_STICK_X, jlx);
  SetControlState(id, Touch::GCPAD_C_STICK_Y, jly);

  // Right controller stick tn GCPad
  if (r & (int)Button::Grip)
  {
    SetControlState(id, Touch::GCPAD_DPAD_UP, r & (int)Button::Up ? 1 : 0);
    SetControlState(id, Touch::GCPAD_DPAD_DOWN, r & (int)Button::Down ? 1 : 0);
    SetControlState(id, Touch::GCPAD_DPAD_LEFT, r & (int)Button::Left ? 1 : 0);
    SetControlState(id, Touch::GCPAD_DPAD_RIGHT, r & (int)Button::Right ? 1 : 0);
  }
  else
  {
    SetControlState(id, Touch::GCPAD_MAIN_STICK_X, jrx);
    SetControlState(id, Touch::GCPAD_MAIN_STICK_Y, jry);
  }

  // Right controller to GCPad
  SetControlState(id, Touch::GCPAD_A_BUTTON, r & (int)Button::Trigger ? 1 : 0);
  SetControlState(id, Touch::GCPAD_B_BUTTON, r & (int)Button::B ? 1 : 0);
  SetControlState(id, Touch::GCPAD_Z_BUTTON, r & (int)Button::A ? 1 : 0);
  SetControlState(id, Touch::GCPAD_START_BUTTON, r & (int)Button::RThumb ? 1 : 0);

  // Left controller to WiiMote + Nunchuk
  SetControlState(id, Touch::WIIMOTE_ONE_BUTTON, l & (int)Button::X ? 1 : 0);
  SetControlState(id, Touch::WIIMOTE_TWO_BUTTON, l & (int)Button::Y ? 1 : 0);
  SetControlState(id, Touch::WIIMOTE_HOME_BUTTON, l & (int)Button::Back ? 1 : 0);
  SetControlState(id, Touch::NUNCHUK_C_BUTTON, l & (int)Button::Trigger ? 1 : 0);
  SetControlState(id, Touch::NUNCHUK_Z_BUTTON, l & (int)Button::Grip ? 1 : 0);
  SetControlState(id, Touch::NUNCHUK_STICK_X, jlx);
  SetControlState(id, Touch::NUNCHUK_STICK_Y, jly);

  // Right controller to WiiMote
  SetControlState(id, Touch::WIIMOTE_A_BUTTON, r & (int)Button::Trigger ? 1 : 0);
  SetControlState(id, Touch::WIIMOTE_B_BUTTON, r & (int)Button::Grip ? 1 : 0);
  SetControlState(id, Touch::WIIMOTE_PLUS_BUTTON, r & (int)Button::A ? 1 : 0);
  SetControlState(id, Touch::WIIMOTE_MINUS_BUTTON, r & (int)Button::B ? 1 : 0);
  SetControlState(id, Touch::WIIMOTE_DPAD_UP, r & (int)Button::Up ? 1 : 0);
  SetControlState(id, Touch::WIIMOTE_DPAD_DOWN, r & (int)Button::Down ? 1 : 0);
  SetControlState(id, Touch::WIIMOTE_DPAD_LEFT, r & (int)Button::Left ? 1 : 0);
  SetControlState(id, Touch::WIIMOTE_DPAD_RIGHT, r & (int)Button::Right ? 1 : 0);
  SetControlState(id, Touch::WIIMOTE_IR_X, x);
  SetControlState(id, Touch::WIIMOTE_IR_Y, y);
}
}  // namespace ciface::VR
