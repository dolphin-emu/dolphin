// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "Common/VR/API.h"
#include "Common/VR/Input.h"
#include "Common/VR/VRBase.h"
#include "Common/VR/VRRenderer.h"

namespace Common::VR
{
Common::VR::Input* s_module_input = NULL;

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
  s_module_input = new Common::VR::Input();
  ALOGV("OpenXR - VR_Init called");
}
#endif

void GetResolutionPerEye(int* width, int* height)
{
  if (VR_GetEngine()->appState.instance)
  {
    VR_GetResolution(VR_GetEngine(), width, height);
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
  VR_SetConfig(VR_CONFIG_VIEWPORT_VALID, false);
  ALOGV("OpenXR - Viewport invalidated");
}

/*
================================================================================

VR rendering integration

================================================================================
*/

void BindFramebuffer()
{
  VR_BindFramebuffer(VR_GetEngine());
}

bool StartRender()
{
  if (!VR_GetConfig(VR_CONFIG_VIEWPORT_VALID))
  {
    VR_InitRenderer(VR_GetEngine(), false);
    VR_SetConfig(VR_CONFIG_VIEWPORT_VALID, true);
  }

  if (VR_InitFrame(VR_GetEngine()))
  {
    // Set render canvas
    VR_SetConfigFloat(VR_CONFIG_CANVAS_DISTANCE, 4.0f);
    VR_SetConfigFloat(VR_CONFIG_CANVAS_ASPECT, 16.0f / 9.0f / 2.0f);
    VR_SetConfig(VR_CONFIG_MODE, VR_MODE_MONO_SCREEN);

    // Update controllers
    s_module_input->Update(VR_GetEngine());
    auto pose = s_module_input->GetPose(1);
    int l = s_module_input->GetButtonState(0);
    int r = s_module_input->GetButtonState(1);
	auto joystick = s_module_input->GetJoystickState(0);
    auto angles = XrQuaternionf_ToEulerAngles(pose.orientation);
    float x = -tan(ToRadians(angles.y - VR_GetConfigFloat(VR_CONFIG_MENU_YAW)));
    float y = -tan(ToRadians(angles.x)) * VR_GetConfigFloat(VR_CONFIG_CANVAS_ASPECT);
    UpdateInput(0, l, r, x, y, joystick.x, joystick.y);
    return true;
  }
  return false;
}

void FinishRender()
{
  VR_FinishFrame(VR_GetEngine());
}

void PreFrameRender(int fbo_index)
{
  VR_BeginFrame(VR_GetEngine(), fbo_index);
}

void PostFrameRender()
{
  VR_EndFrame(VR_GetEngine());
}

int GetFBOIndex()
{
  return VR_GetConfig(VR_CONFIG_CURRENT_FBO);
}
}
