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
  int left = IN_VRGetButtonState(0);
  int right = IN_VRGetButtonState(1);
  auto angles = XrQuaternionf_ToEulerAngles(pose.orientation);
  float x = -tan(ToRadians(angles.y - VR_GetConfigFloat(VR_CONFIG_MENU_YAW)));
  float y = -tan(ToRadians(angles.x)) * VR_GetConfigFloat(VR_CONFIG_CANVAS_ASPECT);

  // Left controller to GCPad
  SetControlState(controller_index, Touch::GCPAD_X_BUTTON, left & xrButton_Trigger ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_Y_BUTTON, left & xrButton_GripTrigger ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_L_ANALOG, left & xrButton_X ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_R_ANALOG, left & xrButton_Y ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_L_DIGITAL, left & xrButton_X ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_R_DIGITAL, left & xrButton_Y ? 1 : 0);

  // Left controller stick tn GCPad
  if (right & xrButton_B)
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
  SetControlState(controller_index, Touch::GCPAD_A_BUTTON, right & xrButton_Trigger ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_B_BUTTON, right & xrButton_GripTrigger ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_Z_BUTTON, right & xrButton_A ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_DPAD_UP, right & xrButton_Up ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_DPAD_DOWN, right & xrButton_Down ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_DPAD_LEFT, right & xrButton_Left ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_DPAD_RIGHT, right & xrButton_Right ? 1 : 0);
  SetControlState(controller_index, Touch::GCPAD_START_BUTTON, right & xrButton_RThumb ? 1 : 0);

  // Left controller to WiiMote + Nunchuk
  SetControlState(controller_index, Touch::WIIMOTE_PLUS_BUTTON, left & xrButton_X ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_MINUS_BUTTON, left & xrButton_Y ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_HOME_BUTTON, left & xrButton_Back ? 1 : 0);
  SetControlState(controller_index, Touch::NUNCHUK_C_BUTTON, left & xrButton_Trigger ? 1 : 0);
  SetControlState(controller_index, Touch::NUNCHUK_Z_BUTTON, left & xrButton_GripTrigger ? 1 : 0);
  SetControlState(controller_index, Touch::NUNCHUK_STICK_X, IN_VRGetJoystickState(0).x);
  SetControlState(controller_index, Touch::NUNCHUK_STICK_Y, IN_VRGetJoystickState(0).y);

  // Right controller to WiiMote
  SetControlState(controller_index, Touch::WIIMOTE_A_BUTTON, right & xrButton_Trigger ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_B_BUTTON, right & xrButton_GripTrigger ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_ONE_BUTTON, right & xrButton_A ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_TWO_BUTTON, right & xrButton_B ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_DPAD_UP, right & xrButton_Up ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_DPAD_DOWN, right & xrButton_Down ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_DPAD_LEFT, right & xrButton_Left ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_DPAD_RIGHT, right & xrButton_Right ? 1 : 0);
  SetControlState(controller_index, Touch::WIIMOTE_IR_X, x);
  SetControlState(controller_index, Touch::WIIMOTE_IR_Y, y);
}
}  // namespace ciface::VR
