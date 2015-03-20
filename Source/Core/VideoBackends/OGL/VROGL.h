// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32
#include <windows.h>
#include "VideoCommon/VR920.h"
#endif
#ifdef HAVE_OCULUSSDK

#include "OVR_Version.h"
#if OVR_MAJOR_VERSION <= 4
#include "Kernel/OVR_Types.h"
#else
#define OVR_DLL_IMPORT
#endif
#include "OVR_CAPI.h"
#include "OVR_CAPI_GL.h"
#if OVR_MAJOR_VERSION >= 5
#include "Extras/OVR_Math.h"
#else
#include "Kernel/OVR_Math.h"
#endif

extern "C"
{
	void ovrhmd_EnableHSWDisplaySDKRender(ovrHmd hmd, ovrBool enabled);
}
#endif

#include "VideoCommon/VR.h"

namespace OGL
{

void VR_ConfigureHMD();
void VR_StartFramebuffer(int target_width, int target_height, GLuint left_texture, GLuint right_texture);
void VR_PresentHMDFrame();
void VR_DrawTimewarpFrame();
void VR_DrawAsyncTimewarpFrame();

}