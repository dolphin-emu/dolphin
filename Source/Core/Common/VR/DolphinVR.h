// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef ANDROID
#include <jni.h>
#endif

namespace Common::VR
{
// VR app flow integration
bool IsEnabled();
#ifdef ANDROID
void InitOnAndroid(JNIEnv* env, jobject obj, const char* vendor, int version, const char* name);
#endif
void GetResolutionPerEye(int* width, int* height);
void SetCallback(void (*callback)(int id, int l, int r, float x, float y, float jlx, float jly,
                                  float jrx, float jry));
void Start(bool firstStart);

// VR state
void GetControllerOrientation(int index, float& pitch, float& yaw, float& roll);
void GetControllerTranslation(int index, float& x, float& y, float& z);

// VR rendering integration
void BindFramebuffer();
bool StartRender();
void FinishRender();
void PreFrameRender(int fbo_index);
void PostFrameRender();
int GetVRFBOIndex();
}  // namespace Common::VR
