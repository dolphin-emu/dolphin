// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/MathUtil.h"

void InitVR();
void NewVRFrame();
void ReadHmdOrientation(float *roll, float *pitch, float *yaw);
void UpdateHeadTrackingIfNeeded();

extern bool g_has_hmd, g_has_rift, g_has_vr920;
extern bool g_new_tracking_frame;
extern Matrix44 g_head_tracking_matrix;
