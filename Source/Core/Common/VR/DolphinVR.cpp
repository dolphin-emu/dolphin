// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "Common/VR/DolphinVR.h"
#include "Common/VR/VRBase.h"
#include "Common/VR/VRInput.h"
#include "Common/VR/VRRenderer.h"

static void (*UpdateInput)(int controller_index);

/*
================================================================================

VR app flow integration

================================================================================
*/

bool IsVREnabled()
{
#ifdef OPENXR
  return true;
#else
  return false;
#endif
}

#ifdef ANDROID
void InitVROnAndroid(JNIEnv* env, jobject obj, const char* vendor, int version, const char* name)
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
  java.Vm = vm;
  java.ActivityObject = env->NewGlobalRef(obj);
  VR_Init(&java, name, version);
  ALOGV("OpenXR - VR_Init called");
}
#endif

void EnterVR(bool firstStart)
{
  if (firstStart)
  {
    engine_t* engine = VR_GetEngine();
    VR_EnterVR(engine);
    IN_VRInit(engine);
    ALOGV("OpenXR - EnterVR called");
  }
  VR_SetConfig(VR_CONFIG_VIEWPORT_VALID, false);
  ALOGV("OpenXR - Viewport invalidated");
}

void GetVRResolutionPerEye(int* width, int* height)
{
  if (VR_GetEngine()->appState.Instance)
  {
    VR_GetResolution(VR_GetEngine(), width, height);
  }
}

void SetVRCallbacks(void (*callback)(int controller_index))
{
  UpdateInput = callback;
}

/*
================================================================================

VR rendering integration

================================================================================
*/

void BindVRFramebuffer()
{
  VR_BindFramebuffer(VR_GetEngine());
}

bool StartVRRender()
{
  if (!VR_GetConfig(VR_CONFIG_VIEWPORT_VALID))
  {
    VR_InitRenderer(VR_GetEngine(), false);
    VR_SetConfig(VR_CONFIG_VIEWPORT_VALID, true);
  }

  if (VR_InitFrame(VR_GetEngine()))
  {
    VR_SetConfigFloat(VR_CONFIG_CANVAS_DISTANCE, 4.0f);
    VR_SetConfigFloat(VR_CONFIG_CANVAS_ASPECT, 16.0f / 9.0f / 2.0f);
    VR_SetConfig(VR_CONFIG_MODE, VR_MODE_MONO_SCREEN);
    UpdateInput(0);
    return true;
  }
  return false;
}

void FinishVRRender()
{
  VR_FinishFrame(VR_GetEngine());
}

void PreVRFrameRender(int fboIndex)
{
  VR_BeginFrame(VR_GetEngine(), fboIndex);
}

void PostVRFrameRender()
{
  VR_EndFrame(VR_GetEngine());
}

int GetVRFBOIndex()
{
  return VR_GetConfig(VR_CONFIG_CURRENT_FBO);
}
