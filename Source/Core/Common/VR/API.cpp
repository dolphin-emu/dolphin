// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "Common/VR/API.h"
#include "Common/VR/Input.h"
#include "Common/VR/Math.h"
#include "Common/VR/Renderer.h"
#include "Common/VR/VRBase.h"

namespace Common::VR
{
Common::VR::Input* s_module_input = NULL;
Common::VR::Renderer* s_module_renderer = NULL;

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
    VR_SetPlatformFLag(VR_PLATFORM_CONTROLLER_PICO, true);
    VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_INSTANCE, true);
  }
  else if ((strcmp(vendor, "Meta") == 0) || (strcmp(vendor, "Oculus") == 0))
  {
    VR_SetPlatformFLag(VR_PLATFORM_CONTROLLER_QUEST, true);
    VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_FOVEATION, false);
    VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_PERFORMANCE, true);
  }

  // Get Java VM
  JavaVM* vm;
  env->GetJavaVM(&vm);

  // Init VR
  vrJava java;
  java.vm = vm;
  java.activity = env->NewGlobalRef(obj);
  VR_Init(&java, name, version);
  ALOGV("OpenXR - VR_Init called");

  // Allocate modules
  s_module_input = new Input();
  s_module_renderer = new Renderer();
}
#endif

void GetResolutionPerEye(int* width, int* height)
{
  if (VR_GetEngine()->app_state.instance)
  {
    s_module_renderer->GetResolution(VR_GetEngine(), width, height);
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
    engine_t* engine = VR_GetEngine();
    VR_EnterVR(engine);
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
  s_module_renderer->BindFramebuffer(VR_GetEngine());
}

bool StartRender()
{
  if (!s_module_renderer->GetConfigInt(CONFIG_VIEWPORT_VALID))
  {
    s_module_renderer->Init(VR_GetEngine(), false);
    s_module_renderer->SetConfigInt(CONFIG_VIEWPORT_VALID, true);
  }

  if (s_module_renderer->InitFrame(VR_GetEngine()))
  {
    // Set render canvas
    s_module_renderer->SetConfigFloat(CONFIG_CANVAS_DISTANCE, 4.0f);
    s_module_renderer->SetConfigFloat(CONFIG_CANVAS_ASPECT, 16.0f / 9.0f / 2.0f);
    s_module_renderer->SetConfigInt(CONFIG_MODE, RENDER_MODE_MONO_SCREEN);

    // Update controllers
    s_module_input->Update(VR_GetEngine());
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
  s_module_renderer->FinishFrame(VR_GetEngine());
}

void PreFrameRender(int fbo_index)
{
  s_module_renderer->BeginFrame(VR_GetEngine(), fbo_index);
}

void PostFrameRender()
{
  s_module_renderer->EndFrame(VR_GetEngine());
}

int GetFBOIndex()
{
  return s_module_renderer->GetConfigInt(CONFIG_CURRENT_FBO);
}
}
