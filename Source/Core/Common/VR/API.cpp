// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "Common/VR/API.h"
#include "Common/VR/Base.h"
#include "Common/VR/Input.h"
#include "Common/VR/Math.h"
#include "Common/VR/Renderer.h"

namespace Common::VR
{
Base* s_module_base = NULL;
Input* s_module_input = NULL;
Renderer* s_module_renderer = NULL;
bool s_platform_flags[PLATFORM_MAX];

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

  // Get Java VM
  JavaVM* vm;
  env->GetJavaVM(&vm);

  // Init VR
  vrJava java;
  java.vm = vm;
  java.activity = env->NewGlobalRef(obj);
  s_module_base->Init(&java, name, version);
  ALOGV("OpenXR - Init called");
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
    ALOGV("OpenXR - EnterVR called");
  }
  s_module_renderer->SetConfigInt(CONFIG_VIEWPORT_VALID, false);
  ALOGV("OpenXR - Viewport invalidated");
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
}
