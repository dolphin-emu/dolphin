// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32
#include <windows.h>
#include "VideoCommon/VR920.h"
#endif
#ifdef HAVE_OCULUSSDK
#define OVR_D3D_VERSION 11

#include "Kernel/OVR_Types.h"
#include "OVR_CAPI.h"
#include "OVR_CAPI_D3D.h"
#include "Kernel/OVR_Math.h"

#include "d3d11.h"

extern "C"
{
	void ovrhmd_EnableHSWDisplaySDKRender(ovrHmd hmd, ovrBool enabled);
}
#endif

#include "VideoCommon/VR.h"

namespace DX11
{

void VR_ConfigureHMD();
void VR_StartFramebuffer();
void VR_PresentHMDFrame();
void VR_DrawTimewarpFrame();

}