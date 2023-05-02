// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/VR/VRInput.h"
#include "Common/VR/VRRenderer.h"

#include "InputCommon/ControllerInterface/Touch/InputOverrider.h"
#include "InputCommon/ControllerInterface/VR/OpenXR.h"

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

void UpdateInput(int controller_index)
{
  // Get controllers state
  auto pose = IN_VRGetPose(1);
  int l = IN_VRGetButtonState(0);
  int r = IN_VRGetButtonState(1);
  auto angles = XrQuaternionf_ToEulerAngles(pose.orientation);
  float x = -tan(ToRadians(angles.y - VR_GetConfigFloat(VR_CONFIG_MENU_YAW)));
  float y = -tan(ToRadians(angles.x)) * VR_GetConfigFloat(VR_CONFIG_CANVAS_ASPECT);

  // Left controller to GCPad
  SetControlState(controller_index, Touch::GCPAD_X_BUTTON, l & (int)xrButton::Trigger ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_Y_BUTTON, l & (int)xrButton::Grip ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_L_ANALOG, l & (int)xrButton::X ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_R_ANALOG, l & (int)xrButton::Y ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_L_DIGITAL, l & (int)xrButton::X ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_R_DIGITAL, l & (int)xrButton::Y ? 1 : 0);

  // Left controller stick tn GCPad
  if (r & (int)xrButton::B)
  {
    SetControlState(controller_index, Touch::GCPAD_C_STICK_X, IN_VRGetJoystickState(0).x);
    SetControlState(controller_index, Touch::GCPAD_C_STICK_Y, IN_VRGetJoystickState(0).y);
  }
  else
  {
    SetControlState(controller_index, Touch::GCPAD_MAIN_STICK_X, IN_VRGetJoystickState(0).x);
    SetControlState(controller_index, Touch::GCPAD_MAIN_STICK_Y, IN_VRGetJoystickState(0).y);
  }

  // Right controller to GCPad
  SetControlState(controller_index, Touch::GCPAD_A_BUTTON, r & (int)xrButton::Trigger ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_B_BUTTON, r & (int)xrButton::Grip ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_Z_BUTTON, r & (int)xrButton::A ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_DPAD_UP, r & (int)xrButton::Up ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_DPAD_DOWN, r & (int)xrButton::Down ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_DPAD_LEFT, r & (int)xrButton::Left ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_DPAD_RIGHT, r & (int)xrButton::Right ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_START_BUTTON, r & (int)xrButton::RThumb ? 1 : 0);

  // Left controller to WiiMote + Nunchuk
  SetControlState(controller_index, Touch::WIIMOTE_PLUS_BUTTON, l & (int)xrButton::X ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_MINUS_BUTTON, l & (int)xrButton::Y ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_HOME_BUTTON, l & (int)xrButton::Back ? 1 : 0);
  SetControlState(controller_index, Touch::NUNCHUK_C_BUTTON, l & (int)xrButton::Trigger ? 1 : 0);
  SetControlState(controller_index, Touch::NUNCHUK_Z_BUTTON, l & (int)xrButton::Grip ? 1 : 0);
  SetControlState(controller_index, Touch::NUNCHUK_STICK_X, IN_VRGetJoystickState(0).x);
  SetControlState(controller_index, Touch::NUNCHUK_STICK_Y, IN_VRGetJoystickState(0).y);

  // Right controller to WiiMote
  SetControlState(controller_index, Touch::WIIMOTE_A_BUTTON, r & (int)xrButton::Trigger ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_B_BUTTON, r & (int)xrButton::Grip ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_ONE_BUTTON, r & (int)xrButton::A ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_TWO_BUTTON, r & (int)xrButton::B ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_DPAD_UP, r & (int)xrButton::Up ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_DPAD_DOWN, r & (int)xrButton::Down ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_DPAD_LEFT, r & (int)xrButton::Left ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_DPAD_RIGHT, r & (int)xrButton::Right ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_IR_X, x);
  SetControlState(controller_index, Touch::WIIMOTE_IR_Y, y);
}
}  // namespace ciface::VR
