// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "Common/VR/DolphinVR.h"
#include "Common/VR/VRBase.h"
#include "Common/VR/VRInput.h"
#include "Common/VR/VRMath.h"
#include "Common/VR/VRRenderer.h"

namespace Common::VR
{
Base* s_module_base = NULL;
Input* s_module_input = NULL;
Renderer* s_module_renderer = NULL;
bool s_platform_flags[PLATFORM_MAX];
bool s_enabled = false;

void OXR_CheckErrors(XrResult result, const char* function)
{
  if (XR_FAILED(result))
  {
    char errorBuffer[XR_MAX_RESULT_STRING_SIZE];
    xrResultToString(s_module_base->GetEngine()->app_state.instance, result, errorBuffer);
    ERROR_LOG_FMT(VR, "error: {} {}", function, errorBuffer);
  }
}

static void (*UpdateInput)(int id, int l, int r, float x, float y, float jlx, float jly, float jrx,
                           float jry);

/*
================================================================================

VR app flow integration

================================================================================
*/

bool IsEnabled()
{
#ifdef OPENXR
  return s_enabled;
#else
  return false;
#endif
}

#ifdef ANDROID
void InitOnAndroid(JNIEnv* env, jobject obj, const char* vendor, int version, const char* name)
{
  // Do not allow second initialization
  if (s_platform_flags[PLATFORM_STATUS_INITIALIZED])
  {
    s_module_renderer->SetConfigInt(ConfigInt::CONFIG_VIEWPORT_VALID, false);
    return;
  }

  // Set platform flags
  if ((strcmp(vendor, "Meta") == 0) || (strcmp(vendor, "Oculus") == 0))
  {
    s_platform_flags[PLATFORM_CONTROLLER_QUEST] = true;
    s_platform_flags[PLATFORM_EXTENSION_PERFORMANCE] = true;
  }

  // Allocate modules
  s_module_base = new Base();
  s_module_input = new Input();
  s_module_renderer = new Renderer();
  s_platform_flags[PLATFORM_STATUS_INITIALIZED] = true;
  s_module_renderer->SetConfigFloat(CONFIG_CANVAS_DISTANCE, 4.0f);

  // Get Java VM
  JavaVM* vm;
  env->GetJavaVM(&vm);

  // Init VR
  vrJava java;
  java.vm = vm;
  java.activity = env->NewGlobalRef(obj);
  s_module_base->Init(&java, name, version);
  s_enabled = true;
  DEBUG_LOG_FMT(VR, "Init called");
}
#endif

bool GetPlatformFlag(PlatformFlag flag)
{
  return s_platform_flags[flag];
}

void GetResolutionPerEye(int* width, int* height)
{
  auto engine = s_module_base->GetEngine();
  if (engine->app_state.instance)
  {
    s_module_renderer->GetResolution(engine, width, height);
  }
}

void SetCallback(void (*callback)(int id, int l, int r, float x, float y, float jlx, float jly,
                                  float jrx, float jry))
{
  UpdateInput = callback;
}

void Start(bool firstStart)
{
  if (firstStart)
  {
    auto engine = s_module_base->GetEngine();
    s_module_base->EnterVR(engine);
    s_module_input->Init(engine);
    DEBUG_LOG_FMT(VR, "EnterVR called");
  }
  s_module_renderer->SetConfigInt(CONFIG_VIEWPORT_VALID, false);
  DEBUG_LOG_FMT(VR, "Viewport invalidated");
}

/*
================================================================================

VR state

================================================================================
*/

void GetControllerOrientation(int index, float& pitch, float& yaw, float& roll)
{
  auto angles = EulerAngles(s_module_input->GetPose(index).orientation);
  pitch = ToRadians(angles.x);
  yaw = ToRadians(angles.y);
  roll = ToRadians(angles.z);
}

void GetControllerTranslation(int index, float& x, float& y, float& z)
{
  auto pos = s_module_input->GetPose(index).position;
  x = -pos.x;
  y = pos.y * 0.5f;
  z = pos.z;
}

/*
================================================================================

VR rendering integration

================================================================================
*/

void BindFramebuffer()
{
  s_module_renderer->BindFramebuffer(s_module_base->GetEngine());
}

bool StartRender()
{
  auto engine = s_module_base->GetEngine();
  if (!s_module_renderer->GetConfigInt(CONFIG_VIEWPORT_VALID))
  {
    s_module_renderer->Init(engine, false);
    s_module_renderer->SetConfigInt(CONFIG_VIEWPORT_VALID, true);
  }

  if (s_module_renderer->InitFrame(engine))
  {
    // Set render canvas
    s_module_renderer->SetConfigFloat(CONFIG_CANVAS_ASPECT, 16.0f / 9.0f / 2.0f);
    s_module_renderer->SetConfigInt(CONFIG_MODE, RENDER_MODE_MONO_SCREEN);

    // Get controllers state
    s_module_input->Update(engine);
    auto pose = s_module_input->GetPose(1);
    int l = s_module_input->GetButtonState(0);
    int r = s_module_input->GetButtonState(1);
    auto joy_l = s_module_input->GetJoystickState(0);
    auto joy_r = s_module_input->GetJoystickState(1);
    auto angles = EulerAngles(pose.orientation);
    float x = -tan(ToRadians(angles.y - s_module_renderer->GetConfigFloat(CONFIG_MENU_YAW)));
    float y = -tan(ToRadians(angles.x)) * s_module_renderer->GetConfigFloat(CONFIG_CANVAS_ASPECT);

    // Change canvas distance
    if (l & (int)Button::Grip)
    {
      float value = s_module_renderer->GetConfigFloat(CONFIG_CANVAS_DISTANCE);
      value += s_module_input->GetJoystickState(1).y * 0.1f;
      value = std::clamp(value, 1.0f, 8.0f);
      s_module_renderer->SetConfigFloat(CONFIG_CANVAS_DISTANCE, value);
    }

    // Update game
    UpdateInput(0, l, r, x, y, joy_l.x, joy_l.y, joy_r.x, joy_r.y);
    return true;
  }
  return false;
}

void FinishRender()
{
  s_module_renderer->FinishFrame(s_module_base->GetEngine());
}

void PreFrameRender(int fbo_index)
{
  s_module_renderer->BeginFrame(s_module_base->GetEngine(), fbo_index);
}

void PostFrameRender()
{
  s_module_renderer->EndFrame(s_module_base->GetEngine());
}

int GetFBOIndex()
{
  return s_module_renderer->GetConfigInt(CONFIG_CURRENT_FBO);
}
}  // namespace Common::VR
