// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef ANDROID
#include <jni.h>
#endif

// VR app flow integration
bool IsVREnabled();
#ifdef ANDROID
void InitVROnAndroid(JNIEnv* env, jobject obj, const char* vendor, int version, const char* name);
#endif
void EnterVR(bool firstStart);
void GetVRResolutionPerEye(int* width, int* height);
void SetVRCallbacks(void (*callback)(int controller_index));

// VR rendering integration
void BindVRFramebuffer();
bool StartVRRender();
void FinishVRRender();
void PreVRFrameRender(int fboIndex);
void PostVRFrameRender();
int GetVRFBOIndex();
