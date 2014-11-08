// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once
#define OCULUSSDK043

// Distances are in metres, angles are in degrees.
const float DEFAULT_VR_UNITS_PER_METRE = 1.0f, DEFAULT_VR_HUD_DISTANCE = 1.5f, DEFAULT_VR_HUD_THICKNESS = 0.5f,
	DEFAULT_VR_HUD_3D_CLOSER = 0.5f,
	DEFAULT_VR_CAMERA_FORWARD = 0.0f, DEFAULT_VR_CAMERA_PITCH = 0.0f, DEFAULT_VR_AIM_DISTANCE = 7.0f, 
	DEFAULT_VR_SCREEN_HEIGHT = 2.0f, DEFAULT_VR_SCREEN_DISTANCE = 1.5f, DEFAULT_VR_SCREEN_THICKNESS = 0.5f, 
	DEFAULT_VR_SCREEN_UP = 0.0f, DEFAULT_VR_SCREEN_RIGHT = 0.0f, DEFAULT_VR_SCREEN_PITCH = 0.0f;

#ifdef HAVE_OCULUSSDK
#include "Kernel/OVR_Types.h"
#include "OVR_CAPI.h"
#include "Kernel/OVR_Math.h"
#endif

#include <mutex>

#include "Common/MathUtil.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define RADIANS_TO_DEGREES(rad) ((float) rad * (float) (180.0 / M_PI))
#define DEGREES_TO_RADIANS(deg) ((float) deg * (float) (M_PI / 180.0))

void InitVR();
void ShutdownVR();
void NewVRFrame();
void ReadHmdOrientation(float *roll, float *pitch, float *yaw, float *x, float *y, float *z);
void UpdateHeadTrackingIfNeeded();

extern bool g_force_vr;
extern bool g_has_hmd, g_has_rift, g_has_vr920;
extern bool g_new_tracking_frame;
extern Matrix44 g_head_tracking_matrix;
extern float g_head_tracking_position[3];
extern int g_hmd_window_width, g_hmd_window_height; 
extern const char *g_hmd_device_name;

extern bool debug_nextScene;

#ifdef HAVE_OCULUSSDK
extern ovrHmd hmd;
extern ovrHmdDesc hmdDesc;
extern ovrFovPort g_eye_fov[2];
extern ovrEyeRenderDesc g_eye_render_desc[2];
extern ovrFrameTiming g_rift_frame_timing;
extern ovrPosef g_eye_poses[2], g_front_eye_poses[2];
extern std::mutex g_ovr_lock;
extern int g_ovr_frameindex;
#endif
