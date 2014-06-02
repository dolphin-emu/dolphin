// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#ifdef HAVE_OCULUSSDK
#include "Kernel/OVR_Types.h"
#include "OVR_CAPI.h"
#include "Kernel/OVR_Math.h"
#endif

#include "Common/MathUtil.h"

void InitVR();
void NewVRFrame();
void ReadHmdOrientation(float *roll, float *pitch, float *yaw);
void UpdateHeadTrackingIfNeeded();

extern bool g_has_hmd, g_has_rift, g_has_vr920;
extern bool g_new_tracking_frame;
extern Matrix44 g_head_tracking_matrix;
extern int g_hmd_window_width, g_hmd_window_height;

#ifdef HAVE_OCULUSSDK
extern ovrHmd hmd;
extern ovrHmdDesc hmdDesc;
extern ovrFovPort g_eye_fov[2];
extern ovrEyeRenderDesc g_eye_render_desc[2];
#endif