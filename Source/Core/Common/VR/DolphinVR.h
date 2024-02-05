// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef ANDROID
#include <jni.h>
#endif

namespace Common::VR
{
enum class Button
{
  A = 0x00000001,  // Set for trigger pulled on the Gear VR and Go Controllers
  B = 0x00000002,
  RThumb = 0x00000004,

  X = 0x00000100,
  Y = 0x00000200,
  LThumb = 0x00000400,

  Up = 0x00010000,
  Down = 0x00020000,
  Left = 0x00040000,
  Right = 0x00080000,
  Enter = 0x00100000,  //< Set for touchpad click on the Go Controller, menu
  // button on Left Quest Controller
  Back = 0x00200000,  //< Back button on the Go Controller (only set when
  // a short press comes up)
  Grip = 0x04000000,    //< grip trigger engaged
  Trigger = 0x20000000  //< Index Trigger engaged
};

enum PlatformFlag
{
  PLATFORM_CONTROLLER_PICO,
  PLATFORM_CONTROLLER_QUEST,
  PLATFORM_EXTENSION_FOVEATION,
  PLATFORM_EXTENSION_INSTANCE,
  PLATFORM_EXTENSION_PERFORMANCE,
  PLATFORM_STATUS_INITIALIZED,
  PLATFORM_TRACKING_FLOOR,
  PLATFORM_MAX
};

// VR app flow integration
bool IsEnabled();
#ifdef ANDROID
void InitOnAndroid(JNIEnv* env, jobject obj, const char* vendor, int version, const char* name);
#endif
bool GetPlatformFlag(PlatformFlag flag);
void GetResolutionPerEye(int* width, int* height);
void SetCallback(void (*callback)(int id, int l, int r, float x, float y, float jlx, float jly, float jrx, float jry));
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
