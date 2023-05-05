// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/VR/Input.h"
#include <cstring>
#include "Common/VR/API.h"

#if !defined(_WIN32)
#include <sys/time.h>
#endif

namespace Common::VR
{
void Input::Init(engine_t* engine)
{
  if (input_initialized)
    return;

  // Actions
  action_set = CreateActionSet(engine->app_state.instance, "running_action_set", "Actionset");
  index_left =
      CreateAction(action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "index_left", "Index left", 0, NULL);
  index_right =
      CreateAction(action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "index_right", "Index right", 0, NULL);
  menu = CreateAction(action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu_action", "Menu", 0, NULL);
  button_a =
      CreateAction(action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a", "Button A", 0, NULL);
  button_b =
      CreateAction(action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b", "Button B", 0, NULL);
  button_x =
      CreateAction(action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x", "Button X", 0, NULL);
  button_y =
      CreateAction(action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y", "Button Y", 0, NULL);
  grip_left =
      CreateAction(action_set, XR_ACTION_TYPE_FLOAT_INPUT, "grip_left", "Grip left", 0, NULL);
  grip_right =
      CreateAction(action_set, XR_ACTION_TYPE_FLOAT_INPUT, "grip_right", "Grip right", 0, NULL);
  joystick_left = CreateAction(action_set, XR_ACTION_TYPE_VECTOR2F_INPUT, "move_on_left_joy",
                               "Move on left Joy", 0, NULL);
  joystick_right = CreateAction(action_set, XR_ACTION_TYPE_VECTOR2F_INPUT, "move_on_right_joy",
                                "Move on right Joy", 0, NULL);
  thumb_left = CreateAction(action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_left",
                            "Thumbstick left", 0, NULL);
  thumb_right = CreateAction(action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_right",
                             "Thumbstick right", 0, NULL);
  vibrate_left_feedback = CreateAction(action_set, XR_ACTION_TYPE_VIBRATION_OUTPUT,
                                       "vibrate_left_feedback", "Vibrate Left Controller", 0, NULL);
  vibrate_right_feedback =
      CreateAction(action_set, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_right_feedback",
                   "Vibrate Right Controller", 0, NULL);

  OXR(xrStringToPath(engine->app_state.instance, "/user/hand/left", &left_hand_path));
  OXR(xrStringToPath(engine->app_state.instance, "/user/hand/right", &right_hand_path));
  hand_pose_left = CreateAction(action_set, XR_ACTION_TYPE_POSE_INPUT, "hand_pose_left", NULL, 1,
                                &left_hand_path);
  hand_pose_right = CreateAction(action_set, XR_ACTION_TYPE_POSE_INPUT, "hand_pose_right", NULL, 1,
                                 &right_hand_path);

  XrPath interactionProfilePath = XR_NULL_PATH;
  if (GetPlatformFlag(PLATFORM_CONTROLLER_QUEST))
  {
    OXR(xrStringToPath(engine->app_state.instance, "/interaction_profiles/oculus/touch_controller",
                       &interactionProfilePath));
  }
  else if (GetPlatformFlag(PLATFORM_CONTROLLER_PICO))
  {
    OXR(xrStringToPath(engine->app_state.instance, "/interaction_profiles/pico/neo3_controller",
                       &interactionProfilePath));
  }

  // Map bindings
  XrInstance instance = engine->app_state.instance;
  XrActionSuggestedBinding bindings[32];  // large enough for all profiles
  int curr = 0;

  if (GetPlatformFlag(PLATFORM_CONTROLLER_QUEST))
  {
    bindings[curr++] = GetBinding(instance, index_left, "/user/hand/left/input/trigger");
    bindings[curr++] = GetBinding(instance, index_right, "/user/hand/right/input/trigger");
    bindings[curr++] = GetBinding(instance, menu, "/user/hand/left/input/menu/click");
  }
  else if (GetPlatformFlag(PLATFORM_CONTROLLER_PICO))
  {
    bindings[curr++] = GetBinding(instance, index_left, "/user/hand/left/input/trigger/click");
    bindings[curr++] = GetBinding(instance, index_right, "/user/hand/right/input/trigger/click");
    bindings[curr++] = GetBinding(instance, menu, "/user/hand/left/input/back/click");
    bindings[curr++] = GetBinding(instance, menu, "/user/hand/right/input/back/click");
  }
  bindings[curr++] = GetBinding(instance, button_x, "/user/hand/left/input/x/click");
  bindings[curr++] = GetBinding(instance, button_y, "/user/hand/left/input/y/click");
  bindings[curr++] = GetBinding(instance, button_a, "/user/hand/right/input/a/click");
  bindings[curr++] = GetBinding(instance, button_b, "/user/hand/right/input/b/click");
  bindings[curr++] = GetBinding(instance, grip_left, "/user/hand/left/input/squeeze/value");
  bindings[curr++] = GetBinding(instance, grip_right, "/user/hand/right/input/squeeze/value");
  bindings[curr++] = GetBinding(instance, joystick_left, "/user/hand/left/input/thumbstick");
  bindings[curr++] = GetBinding(instance, joystick_right, "/user/hand/right/input/thumbstick");
  bindings[curr++] = GetBinding(instance, thumb_left, "/user/hand/left/input/thumbstick/click");
  bindings[curr++] = GetBinding(instance, thumb_right, "/user/hand/right/input/thumbstick/click");
  bindings[curr++] = GetBinding(instance, vibrate_left_feedback, "/user/hand/left/output/haptic");
  bindings[curr++] = GetBinding(instance, vibrate_right_feedback, "/user/hand/right/output/haptic");
  bindings[curr++] = GetBinding(instance, hand_pose_left, "/user/hand/left/input/aim/pose");
  bindings[curr++] = GetBinding(instance, hand_pose_right, "/user/hand/right/input/aim/pose");

  XrInteractionProfileSuggestedBinding suggested_bindings = {};
  suggested_bindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
  suggested_bindings.next = NULL;
  suggested_bindings.interactionProfile = interactionProfilePath;
  suggested_bindings.suggestedBindings = bindings;
  suggested_bindings.countSuggestedBindings = curr;
  OXR(xrSuggestInteractionProfileBindings(engine->app_state.instance, &suggested_bindings));

  // Attach actions
  XrSessionActionSetsAttachInfo attach_info = {};
  attach_info.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
  attach_info.next = NULL;
  attach_info.countActionSets = 1;
  attach_info.actionSets = &action_set;
  OXR(xrAttachSessionActionSets(engine->app_state.session, &attach_info));

  // Enumerate actions
  char string_buffer[256];
  XrPath action_paths_buffer[32];
  XrAction actions_to_enumerate[] = {index_left,
                                     index_right,
                                     menu,
                                     button_a,
                                     button_b,
                                     button_x,
                                     button_y,
                                     grip_left,
                                     grip_right,
                                     joystick_left,
                                     joystick_right,
                                     thumb_left,
                                     thumb_right,
                                     vibrate_left_feedback,
                                     vibrate_right_feedback,
                                     hand_pose_left,
                                     hand_pose_right};
  for (XrAction& i : actions_to_enumerate)
  {
    XrBoundSourcesForActionEnumerateInfo e = {};
    e.type = XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO;
    e.next = NULL;
    e.action = i;

    // Get Count
    uint32_t count_output = 0;
    OXR(xrEnumerateBoundSourcesForAction(engine->app_state.session, &e, 0, &count_output, NULL));

    if (count_output < 32)
    {
      OXR(xrEnumerateBoundSourcesForAction(engine->app_state.session, &e, 32, &count_output,
                                           action_paths_buffer));
      for (uint32_t a = 0; a < count_output; ++a)
      {
        XrInputSourceLocalizedNameGetInfo name_info = {};
        name_info.type = XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO;
        name_info.next = NULL;
        name_info.sourcePath = action_paths_buffer[a];
        name_info.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                    XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                    XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;

        uint32_t str_count = 0u;
        OXR(xrGetInputSourceLocalizedName(engine->app_state.session, &name_info, 0, &str_count,
                                          NULL));
        if (str_count < 256)
        {
          OXR(xrGetInputSourceLocalizedName(engine->app_state.session, &name_info, 256, &str_count,
                                            string_buffer));
          char path_str[256];
          uint32_t str_len = 0;
          OXR(xrPathToString(engine->app_state.instance, action_paths_buffer[a],
                             (uint32_t)sizeof(path_str), &str_len, path_str));
          ALOGV("  -> path = %lld `%s` -> `%s`", (long long)action_paths_buffer[a], path_str,
                string_buffer);
        }
      }
    }
  }
  input_initialized = true;
}

uint32_t Input::GetButtonState(int controller)
{
  switch (controller)
  {
  case 0:
    return buttons_left;
  case 1:
    return buttons_right;
  default:
    return 0;
  }
}

XrVector2f Input::GetJoystickState(int controller)
{
  return move_joystick_state[controller].currentState;
}

XrPosef Input::GetPose(int controller)
{
  return controller_pose[controller].pose;
}

void Input::Update(engine_t* engine)
{
  // sync action data
  XrActiveActionSet activeActionSet = {};
  activeActionSet.actionSet = action_set;
  activeActionSet.subactionPath = XR_NULL_PATH;

  XrActionsSyncInfo sync_info = {};
  sync_info.type = XR_TYPE_ACTIONS_SYNC_INFO;
  sync_info.next = NULL;
  sync_info.countActiveActionSets = 1;
  sync_info.activeActionSets = &activeActionSet;
  OXR(xrSyncActions(engine->app_state.session, &sync_info));

  // query input action states
  XrActionStateGetInfo get_info = {};
  get_info.type = XR_TYPE_ACTION_STATE_GET_INFO;
  get_info.next = NULL;
  get_info.subactionPath = XR_NULL_PATH;

  XrSession session = engine->app_state.session;
  ProcessHaptics(session);

  if (left_controller_space == XR_NULL_HANDLE)
  {
    left_controller_space = CreateActionSpace(session, hand_pose_left, left_hand_path);
  }
  if (right_controller_space == XR_NULL_HANDLE)
  {
    right_controller_space = CreateActionSpace(session, hand_pose_right, right_hand_path);
  }

  // button mapping
  buttons_left = 0;
  if (GetActionStateBoolean(session, menu).currentState)
    buttons_left |= (int)Button::Enter;
  if (GetActionStateBoolean(session, button_x).currentState)
    buttons_left |= (int)Button::X;
  if (GetActionStateBoolean(session, button_y).currentState)
    buttons_left |= (int)Button::Y;
  if (GetActionStateBoolean(session, index_left).currentState)
    buttons_left |= (int)Button::Trigger;
  if (GetActionStateFloat(session, grip_left).currentState > 0.5f)
    buttons_left |= (int)Button::Grip;
  if (GetActionStateBoolean(session, thumb_left).currentState)
    buttons_left |= (int)Button::LThumb;
  buttons_right = 0;
  if (GetActionStateBoolean(session, button_a).currentState)
    buttons_right |= (int)Button::A;
  if (GetActionStateBoolean(session, button_b).currentState)
    buttons_right |= (int)Button::B;
  if (GetActionStateBoolean(session, index_right).currentState)
    buttons_right |= (int)Button::Trigger;
  if (GetActionStateFloat(session, grip_right).currentState > 0.5f)
    buttons_right |= (int)Button::Grip;
  if (GetActionStateBoolean(session, thumb_right).currentState)
    buttons_right |= (int)Button::RThumb;

  // thumbstick
  move_joystick_state[0] = GetActionStateVector2(session, joystick_left);
  move_joystick_state[1] = GetActionStateVector2(session, joystick_right);
  if (move_joystick_state[0].currentState.x > 0.5)
    buttons_left |= (int)Button::Right;
  if (move_joystick_state[0].currentState.x < -0.5)
    buttons_left |= (int)Button::Left;
  if (move_joystick_state[0].currentState.y > 0.5)
    buttons_left |= (int)Button::Up;
  if (move_joystick_state[0].currentState.y < -0.5)
    buttons_left |= (int)Button::Down;
  if (move_joystick_state[1].currentState.x > 0.5)
    buttons_right |= (int)Button::Right;
  if (move_joystick_state[1].currentState.x < -0.5)
    buttons_right |= (int)Button::Left;
  if (move_joystick_state[1].currentState.y > 0.5)
    buttons_right |= (int)Button::Up;
  if (move_joystick_state[1].currentState.y < -0.5)
    buttons_right |= (int)Button::Down;

  // pose
  for (int i = 0; i < 2; i++)
  {
    controller_pose[i] = {};
    controller_pose[i].type = XR_TYPE_SPACE_LOCATION;
    XrSpace aim_space[] = {left_controller_space, right_controller_space};
    xrLocateSpace(aim_space[i], engine->app_state.current_space,
                  (XrTime)(engine->predicted_display_time), &controller_pose[i]);
  }
}

void Input::Vibrate(int duration, int chan, float intensity)
{
  for (int i = 0; i < 2; ++i)
  {
    int channel = i & chan;
    if (channel)
    {
      if (vibration_channel_duration[channel] > 0.0f)
        return;

      if (vibration_channel_duration[channel] == -1.0f && duration != 0.0f)
        return;

      vibration_channel_duration[channel] = (float)duration;
      vibration_channel_intensity[channel] = intensity;
    }
  }
}

XrAction Input::CreateAction(XrActionSet output_set, XrActionType type, const char* actionName,
                             const char* description, int count_subaction_path,
                             XrPath* subaction_path)
{
  XrActionCreateInfo action_info = {};
  action_info.type = XR_TYPE_ACTION_CREATE_INFO;
  action_info.next = NULL;
  action_info.actionType = type;
  if (count_subaction_path > 0)
  {
    action_info.countSubactionPaths = count_subaction_path;
    action_info.subactionPaths = subaction_path;
  }
  strcpy(action_info.actionName, actionName);
  strcpy(action_info.localizedActionName, description ? description : actionName);
  XrAction output = XR_NULL_HANDLE;
  OXR(xrCreateAction(output_set, &action_info, &output));
  return output;
}

XrActionSet Input::CreateActionSet(XrInstance instance, const char* name, const char* description)
{
  XrActionSetCreateInfo action_set_info = {};
  action_set_info.type = XR_TYPE_ACTION_SET_CREATE_INFO;
  action_set_info.next = NULL;
  action_set_info.priority = 1;
  strcpy(action_set_info.actionSetName, name);
  strcpy(action_set_info.localizedActionSetName, description);
  XrActionSet output = XR_NULL_HANDLE;
  OXR(xrCreateActionSet(instance, &action_set_info, &output));
  return output;
}

XrSpace Input::CreateActionSpace(XrSession session, XrAction pose_action, XrPath subaction_path)
{
  XrActionSpaceCreateInfo action_space_info = {};
  action_space_info.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
  action_space_info.action = pose_action;
  action_space_info.poseInActionSpace.orientation.w = 1.0f;
  action_space_info.subactionPath = subaction_path;
  XrSpace output = XR_NULL_HANDLE;
  OXR(xrCreateActionSpace(session, &action_space_info, &output));
  return output;
}

XrActionStateBoolean Input::GetActionStateBoolean(XrSession session, XrAction action)
{
  XrActionStateGetInfo get_info = {};
  get_info.type = XR_TYPE_ACTION_STATE_GET_INFO;
  get_info.action = action;

  XrActionStateBoolean state = {};
  state.type = XR_TYPE_ACTION_STATE_BOOLEAN;

  OXR(xrGetActionStateBoolean(session, &get_info, &state));
  return state;
}

XrActionStateFloat Input::GetActionStateFloat(XrSession session, XrAction action)
{
  XrActionStateGetInfo get_info = {};
  get_info.type = XR_TYPE_ACTION_STATE_GET_INFO;
  get_info.action = action;

  XrActionStateFloat state = {};
  state.type = XR_TYPE_ACTION_STATE_FLOAT;

  OXR(xrGetActionStateFloat(session, &get_info, &state));
  return state;
}

XrActionStateVector2f Input::GetActionStateVector2(XrSession session, XrAction action)
{
  XrActionStateGetInfo get_info = {};
  get_info.type = XR_TYPE_ACTION_STATE_GET_INFO;
  get_info.action = action;

  XrActionStateVector2f state = {};
  state.type = XR_TYPE_ACTION_STATE_VECTOR2F;

  OXR(xrGetActionStateVector2f(session, &get_info, &state));
  return state;
}

XrActionSuggestedBinding Input::GetBinding(XrInstance instance, XrAction action, const char* name)
{
  XrPath bindingPath;
  OXR(xrStringToPath(instance, name, &bindingPath));

  XrActionSuggestedBinding output;
  output.action = action;
  output.binding = bindingPath;
  return output;
}

int Input::GetMilliseconds()
{
#if !defined(_WIN32)
  struct timeval tp;

  gettimeofday(&tp, NULL);

  if (!sys_timeBase)
  {
    sys_timeBase = tp.tv_sec;
    return tp.tv_usec / 1000;
  }

  return (tp.tv_sec - sys_timeBase) * 1000 + tp.tv_usec / 1000;
#else
  if (frequency.QuadPart == 0)
  {
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start_time);
    frequency_mult = 1.0 / static_cast<double>(frequency.QuadPart);
  }
  LARGE_INTEGER time;
  QueryPerformanceCounter(&time);
  double elapsed = static_cast<double>(time.QuadPart - start_time.QuadPart);
  return (int)(elapsed * frequency_mult * 1000.0);
#endif
}

void Input::ProcessHaptics(XrSession session)
{
  static float last_frame_timestamp = 0.0f;
  float timestamp = (float)(GetMilliseconds());
  float frametime = timestamp - last_frame_timestamp;
  last_frame_timestamp = timestamp;

  for (int i = 0; i < 2; ++i)
  {
    if (vibration_channel_duration[i] > 0.0f || vibration_channel_duration[i] == -1.0f)
    {
      // fire haptics using output action
      XrHapticVibration vibration = {};
      vibration.type = XR_TYPE_HAPTIC_VIBRATION;
      vibration.next = NULL;
      vibration.amplitude = vibration_channel_intensity[i];
      vibration.duration = ToXrTime(vibration_channel_duration[i]);
      vibration.frequency = 3000;
      XrHapticActionInfo haptic_info = {};
      haptic_info.type = XR_TYPE_HAPTIC_ACTION_INFO;
      haptic_info.next = NULL;
      haptic_info.action = i == 0 ? vibrate_left_feedback : vibrate_right_feedback;
      OXR(xrApplyHapticFeedback(session, &haptic_info, (const XrHapticBaseHeader*)&vibration));

      if (vibration_channel_duration[i] != -1.0f)
      {
        vibration_channel_duration[i] -= frametime;

        if (vibration_channel_duration[i] < 0.0f)
        {
          vibration_channel_duration[i] = 0.0f;
          vibration_channel_intensity[i] = 0.0f;
        }
      }
    }
    else
    {
      // Stop haptics
      XrHapticActionInfo haptic_info = {};
      haptic_info.type = XR_TYPE_HAPTIC_ACTION_INFO;
      haptic_info.next = NULL;
      haptic_info.action = i == 0 ? vibrate_left_feedback : vibrate_right_feedback;
      OXR(xrStopHapticFeedback(session, &haptic_info));
    }
  }
}

XrTime Input::ToXrTime(const double time_in_seconds)
{
  return (XrTime)(time_in_seconds * 1e9);
}
}  // namespace Common::VR
