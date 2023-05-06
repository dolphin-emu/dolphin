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

void OXR_CheckErrors(XrResult result, const char* function)
{
  if (XR_FAILED(result))
  {
    char errorBuffer[XR_MAX_RESULT_STRING_SIZE];
    xrResultToString(s_module_base->GetEngine()->app_state.instance, result, errorBuffer);
    ERROR_LOG_FMT(VR, "error: {} {}", function, errorBuffer);
  }
}

static void (*UpdateInput)(int id, int l, int r, float x, float y, float jx, float jy);

/*
================================================================================

VR app flow integration

================================================================================
*/

bool IsEnabled()
{
#ifdef OPENXR
  return true;
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
  if (strcmp(vendor, "Pico") == 0)
  {
    s_platform_flags[PLATFORM_CONTROLLER_PICO] = true;
    s_platform_flags[PLATFORM_EXTENSION_INSTANCE] = true;
  }
  else if ((strcmp(vendor, "Meta") == 0) || (strcmp(vendor, "Oculus") == 0))
  {
    s_platform_flags[PLATFORM_CONTROLLER_QUEST] = true;
    s_platform_flags[PLATFORM_EXTENSION_FOVEATION] = false;
    s_platform_flags[PLATFORM_EXTENSION_PERFORMANCE] = true;
  }

  // Allocate modules
  s_module_base = new Base();
  s_module_input = new Input();
  s_module_renderer = new Renderer();
  s_platform_flags[PLATFORM_STATUS_INITIALIZED] = true;

  // Get Java VM
  JavaVM* vm;
  env->GetJavaVM(&vm);

  // Init VR
  vrJava java;
  java.vm = vm;
  java.activity = env->NewGlobalRef(obj);
  s_module_base->Init(&java, name, version);
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

void SetCallback(void (*callback)(int id, int l, int r, float x, float y, float jx, float jy))
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
    s_module_renderer->SetConfigFloat(CONFIG_CANVAS_DISTANCE, 4.0f);
    s_module_renderer->SetConfigFloat(CONFIG_CANVAS_ASPECT, 16.0f / 9.0f / 2.0f);
    s_module_renderer->SetConfigInt(CONFIG_MODE, RENDER_MODE_MONO_SCREEN);

    // Update controllers
    s_module_input->Update(engine);
    auto pose = s_module_input->GetPose(1);
    int l = s_module_input->GetButtonState(0);
    int r = s_module_input->GetButtonState(1);
    auto joystick = s_module_input->GetJoystickState(0);
    auto angles = EulerAngles(pose.orientation);
    float x = -tan(ToRadians(angles.y - s_module_renderer->GetConfigFloat(CONFIG_MENU_YAW)));
    float y = -tan(ToRadians(angles.x)) * s_module_renderer->GetConfigFloat(CONFIG_CANVAS_ASPECT);
    UpdateInput(0, l, r, x, y, joystick.x, joystick.y);
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
