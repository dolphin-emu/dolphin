// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32
#include <windows.h>
#include "VideoCommon/VR920.h"
#endif

#include "VideoCommon/VR.h"

#ifdef HAVE_OCULUSSDK
#include "OVR_CAPI_GL.h"
#else
#endif

namespace OGL
{

void VR_ConfigureHMD();
void VR_StartFramebuffer(int target_width, int target_height, GLuint left_texture, GLuint right_texture);
void VR_PresentHMDFrame();
void VR_DrawTimewarpFrame();
void VR_DrawAsyncTimewarpFrame();

}