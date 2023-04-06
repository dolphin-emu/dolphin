#include <cstring>

#include "Common/VR/DolphinVR.h"
#include "Common/VR/VRBase.h"
#include "Common/VR/VRInput.h"
#include "Common/VR/VRRenderer.h"

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
void InitVROnAndroid(void* vm, void* activity, const char* system, int version, const char* name)
{
  // Get device vendor (uppercase)
  char vendor[64];
  sscanf(system, "%[^:]", vendor);
  for (unsigned int i = 0; i < strlen(vendor); i++)
  {
    if ((vendor[i] >= 'a') && (vendor[i] <= 'z'))
    {
      vendor[i] = vendor[i] - 'a' + 'A';
    }
  }

  // Set platform flags
  if (strcmp(vendor, "PICO") == 0)
  {
    VR_SetPlatformFLag(VR_PLATFORM_CONTROLLER_PICO, true);
    VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_INSTANCE, true);
  }
  else if ((strcmp(vendor, "META") == 0) || (strcmp(vendor, "OCULUS") == 0))
  {
    VR_SetPlatformFLag(VR_PLATFORM_CONTROLLER_QUEST, true);
    VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_FOVEATION, true);
    VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_PERFORMANCE, true);
  }

  // Init VR
  ovrJava java;
  java.Vm = (JavaVM*)vm;
  java.ActivityObject = (jobject)activity;
  VR_Init(&java, name, version);
}
#endif

void EnterVR(bool firstStart)
{
  if (firstStart)
  {
    engine_t* engine = VR_GetEngine();
    VR_EnterVR(engine);
    IN_VRInit(engine);
  }
  VR_SetConfig(VR_CONFIG_VIEWPORT_VALID, false);
}

void GetVRResolutionPerEye(int* width, int* height)
{
  if (VR_GetEngine()->appState.Instance)
  {
    VR_GetResolution(VR_GetEngine(), width, height);
  }
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
    VR_SetConfigFloat(VR_CONFIG_CANVAS_DISTANCE, 12.0f);
    VR_SetConfigFloat(VR_CONFIG_CANVAS_ASPECT, 16.0f / 9.0f);
    VR_SetConfig(VR_CONFIG_MODE, VR_MODE_MONO_SCREEN);
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
