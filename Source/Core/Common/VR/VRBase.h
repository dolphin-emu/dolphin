// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Logging/Log.h"
#include "Common/VR/OpenXRLoader.h"

#define _USE_MATH_DEFINES
#include <cassert>
#include <cmath>

#if defined(_DEBUG) &&                                                                             \
    (defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES))

void GLCheckErrors(const char* file, int line);

#define GL(func)                                                                                   \
  func;                                                                                            \
  GLCheckErrors(__FILE__, __LINE__);
#else
#define GL(func) func;
#endif

#if defined(_DEBUG) && defined(ANDROID)
void OXR_CheckErrors(XrResult result, const char* function)
#define OXR(func) OXR_CheckErrors(func, #func);
#else
#define OXR(func) func;
#endif

    namespace Common::VR
{
  enum
  {
    MaxLayerCount = 2
  };
  enum
  {
    MaxNumEyes = 2
  };

  typedef union
  {
    XrCompositionLayerProjection projection;
    XrCompositionLayerCylinderKHR cylinder;
  } CompositorLayer;

  typedef struct
  {
    XrSwapchain handle;
    uint32_t width;
    uint32_t height;
  } SwapChain;

  typedef struct
  {
    int width;
    int height;
    bool acquired;

    uint32_t swapchain_index;
    uint32_t swapchain_length;
    SwapChain swapchain_color;
    void* swapchain_image;

    unsigned int* gl_depth_buffers;
    unsigned int* gl_frame_buffers;
  } Framebuffer;

  typedef struct
  {
    bool multiview;
    Framebuffer framebuffer[MaxNumEyes];
  } Display;

  typedef struct
  {
    XrInstance instance;
    XrSession session;
    bool session_active;
    bool session_focused;
    XrSystemId system_id;

    XrSpace current_space;
    XrSpace fake_space;
    XrSpace head_space;
    XrSpace stage_space;

    int layer_count;
    int main_thread_id;
    int render_thread_id;
    int swap_interval;
    CompositorLayer layers[MaxLayerCount];
    XrViewConfigurationProperties viewport_config;
    XrViewConfigurationView view_config[MaxNumEyes];

    Display renderer;
  } App;

#ifdef ANDROID
  typedef struct
  {
    jobject activity;
    JNIEnv* env;
    JavaVM* vm;
  } vrJava;
#endif

  typedef struct
  {
    App app_state;
    uint64_t frame_index;
    XrTime predicted_display_time;
  } engine_t;

  class Base
  {
  public:
    void Init(void* system, const char* name, int version);
    void Destroy(engine_t* engine);
    void EnterVR(engine_t* engine);
    void LeaveVR(engine_t* engine);
    engine_t* GetEngine();

  private:
    engine_t m_engine;
    bool m_initialized = false;
  };
}  // namespace Common::VR
