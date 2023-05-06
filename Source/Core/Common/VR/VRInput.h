// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/VR/VRBase.h"

namespace Common::VR
{
class Input
{
public:
  void Init(engine_t* engine);
  uint32_t GetButtonState(int controller);
  XrVector2f GetJoystickState(int controller);
  XrPosef GetPose(int controller);
  void Update(engine_t* engine);
  void Vibrate(int duration, int chan, float intensity);

private:
  XrAction CreateAction(XrActionSet output_set, XrActionType type, const char* name,
                        const char* desc, int count_subaction_path, XrPath* subaction_path);
  XrActionSet CreateActionSet(XrInstance instance, const char* name, const char* desc);
  XrSpace CreateActionSpace(XrSession session, XrAction action, XrPath subaction_path);
  XrActionStateBoolean GetActionStateBoolean(XrSession session, XrAction action);
  XrActionStateFloat GetActionStateFloat(XrSession session, XrAction action);
  XrActionStateVector2f GetActionStateVector2(XrSession session, XrAction action);
  XrActionSuggestedBinding GetBinding(XrInstance instance, XrAction action, const char* name);
  int GetMilliseconds();
  void ProcessHaptics(XrSession session);
  XrTime ToXrTime(const double time_in_seconds);

private:
  bool input_initialized = false;

  // OpenXR controller mapping
  XrActionSet action_set;
  XrPath left_hand_path;
  XrPath right_hand_path;
  XrAction hand_pose_left;
  XrAction hand_pose_right;
  XrAction index_left;
  XrAction index_right;
  XrAction menu;
  XrAction button_a;
  XrAction button_b;
  XrAction button_x;
  XrAction button_y;
  XrAction grip_left;
  XrAction grip_right;
  XrAction joystick_left;
  XrAction joystick_right;
  XrAction thumb_left;
  XrAction thumb_right;
  XrAction vibrate_left_feedback;
  XrAction vibrate_right_feedback;
  XrSpace left_controller_space = XR_NULL_HANDLE;
  XrSpace right_controller_space = XR_NULL_HANDLE;

  // Controller state
  uint32_t buttons_left = 0;
  uint32_t buttons_right = 0;
  XrSpaceLocation controller_pose[2];
  XrActionStateVector2f move_joystick_state[2];
  float vibration_channel_duration[2] = {};
  float vibration_channel_intensity[2] = {};

  // Timer
#if !defined(_WIN32)
  unsigned long sys_timeBase = 0;
#else
  LARGE_INTEGER frequency;
  double frequency_mult;
  LARGE_INTEGER start_time;
#endif
};
}  // namespace Common::VR
