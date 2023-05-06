// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/VR/VRDisplay.h"
#include "Common/VR/DolphinVR.h"

#if XR_USE_GRAPHICS_API_OPENGL_ES

#include <Common/GL/GLExtensions/ARB_framebuffer_object.h>
#include <Common/GL/GLExtensions/ARB_texture_storage.h>
#include <Common/GL/GLExtensions/gl_1_1.h>
#include <Common/GL/GLExtensions/gl_1_2.h>
#include <Common/GL/GLExtensions/gl_2_1.h>
#include <Common/GL/GLExtensions/gl_3_0.h>
#include <Common/GL/GLExtensions/gl_common.h>

#endif

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#if !defined(_WIN32)
#include <pthread.h>
#endif

double FromXrTime(const XrTime time)
{
  return (time * 1e-9);
}

#if XR_USE_GRAPHICS_API_OPENGL || XR_USE_GRAPHICS_API_OPENGL_ES

static const char* GlErrorString(GLenum error)
{
  switch (error)
  {
  case GL_NO_ERROR:
    return "GL_NO_ERROR";
  case GL_INVALID_ENUM:
    return "GL_INVALID_ENUM";
  case GL_INVALID_VALUE:
    return "GL_INVALID_VALUE";
  case GL_INVALID_OPERATION:
    return "GL_INVALID_OPERATION";
  case GL_INVALID_FRAMEBUFFER_OPERATION:
    return "GL_INVALID_FRAMEBUFFER_OPERATION";
  case GL_OUT_OF_MEMORY:
    return "GL_OUT_OF_MEMORY";
  default:
    return "unknown";
  }
}

void GLCheckErrors(const char* file, int line)
{
  for (int i = 0; i < 10; i++)
  {
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR)
    {
      break;
    }
    ERROR_LOG_FMT(VR, "GL error on line {}:{} {}", file, line, GlErrorString(error));
  }
}

#endif

namespace Common::VR
{
/*
================================================================================

ovrFramebuffer

================================================================================
*/

void FramebufferClear(Framebuffer* frameBuffer)
{
  frameBuffer->width = 0;
  frameBuffer->height = 0;
  frameBuffer->swapchain_length = 0;
  frameBuffer->swapchain_index = 0;
  frameBuffer->swapchain_color.handle = XR_NULL_HANDLE;
  frameBuffer->swapchain_color.width = 0;
  frameBuffer->swapchain_color.height = 0;
  frameBuffer->swapchain_image = NULL;

  frameBuffer->gl_depth_buffers = NULL;
  frameBuffer->gl_frame_buffers = NULL;
  frameBuffer->acquired = false;
}

#if XR_USE_GRAPHICS_API_OPENGL_ES

bool FramebufferCreateGLES(XrSession session, Framebuffer* frameBuffer, int width, int height,
                           bool multiview)
{
  frameBuffer->width = width;
  frameBuffer->height = height;

  if (strstr((const char*)glGetString(GL_EXTENSIONS), "GL_OVR_multiview2") == nullptr)
  {
    ERROR_LOG_FMT(VR, "OpenGL implementation does not support GL_OVR_multiview2 extension");
  }

  typedef void (*PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR)(GLenum, GLenum, GLuint, GLint, GLint,
                                                      GLsizei);
  PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR glFramebufferTextureMultiviewOVR;
  glFramebufferTextureMultiviewOVR =
      (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR)eglGetProcAddress("glFramebufferTextureMultiviewOVR");
  if (!glFramebufferTextureMultiviewOVR)
  {
    ERROR_LOG_FMT(VR, "Can not get proc address for glFramebufferTextureMultiviewOVR");
  }
  XrSwapchainCreateInfo swapchain_info;
  memset(&swapchain_info, 0, sizeof(swapchain_info));
  swapchain_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
  swapchain_info.sampleCount = 1;
  swapchain_info.width = width;
  swapchain_info.height = height;
  swapchain_info.faceCount = 1;
  swapchain_info.mipCount = 1;
  swapchain_info.arraySize = multiview ? 2 : 1;

#ifdef ANDROID
  if (GetPlatformFlag(PLATFORM_EXTENSION_FOVEATION))
  {
    XrSwapchainCreateInfoFoveationFB swapchain_foveation_info;
    memset(&swapchain_foveation_info, 0, sizeof(swapchain_foveation_info));
    swapchain_foveation_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB;
    swapchain_info.next = &swapchain_foveation_info;
  }
#endif

  frameBuffer->swapchain_color.width = swapchain_info.width;
  frameBuffer->swapchain_color.height = swapchain_info.height;

  // Create the color swapchain.
  swapchain_info.format = GL_SRGB8_ALPHA8;
  swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
  OXR(xrCreateSwapchain(session, &swapchain_info, &frameBuffer->swapchain_color.handle));
  OXR(xrEnumerateSwapchainImages(frameBuffer->swapchain_color.handle, 0,
                                 &frameBuffer->swapchain_length, NULL));
  frameBuffer->swapchain_image =
      malloc(frameBuffer->swapchain_length * sizeof(XrSwapchainImageOpenGLESKHR));

  // Populate the swapchain image array.
  for (uint32_t i = 0; i < frameBuffer->swapchain_length; i++)
  {
    XrSwapchainImageOpenGLESKHR* swapchain =
        (XrSwapchainImageOpenGLESKHR*)frameBuffer->swapchain_image;
    swapchain[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
    swapchain[i].next = NULL;
  }
  OXR(xrEnumerateSwapchainImages(frameBuffer->swapchain_color.handle, frameBuffer->swapchain_length,
                                 &frameBuffer->swapchain_length,
                                 (XrSwapchainImageBaseHeader*)frameBuffer->swapchain_image));

  frameBuffer->gl_depth_buffers = (GLuint*)malloc(frameBuffer->swapchain_length * sizeof(GLuint));
  frameBuffer->gl_frame_buffers = (GLuint*)malloc(frameBuffer->swapchain_length * sizeof(GLuint));
  for (uint32_t i = 0; i < frameBuffer->swapchain_length; i++)
  {
    // Create color texture.
    GLuint color_texture = ((XrSwapchainImageOpenGLESKHR*)frameBuffer->swapchain_image)[i].image;
    GLenum color_texture_target = multiview ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
    GL(glBindTexture(color_texture_target, color_texture));
    GL(glTexParameteri(color_texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(color_texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(color_texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(color_texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glBindTexture(color_texture_target, 0));

    // Create depth buffer.
    if (multiview)
    {
      GL(glGenTextures(1, &frameBuffer->gl_depth_buffers[i]));
      GL(glBindTexture(GL_TEXTURE_2D_ARRAY, frameBuffer->gl_depth_buffers[i]));
      GL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH24_STENCIL8, width, height, 2));
      GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));
    }
    else
    {
      GL(glGenRenderbuffers(1, &frameBuffer->gl_depth_buffers[i]));
      GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->gl_depth_buffers[i]));
      GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height));
      GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
    }

    // Create the frame buffer.
    GL(glGenFramebuffers(1, &frameBuffer->gl_frame_buffers[i]));
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->gl_frame_buffers[i]));
    if (multiview)
    {
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                          frameBuffer->gl_depth_buffers[i], 0, 0, 2));
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          frameBuffer->gl_depth_buffers[i], 0, 0, 2));
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color_texture,
                                          0, 0, 2));
    }
    else
    {
      GL(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                   frameBuffer->gl_depth_buffers[i]));
      GL(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                   frameBuffer->gl_depth_buffers[i]));
      GL(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                color_texture, 0));
    }
    GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
    if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
      ERROR_LOG_FMT(VR, "Incomplete frame buffer object: {}", renderFramebufferStatus);
      return false;
    }
  }

  return true;
}

#endif

void FramebufferDestroy(Framebuffer* frameBuffer)
{
#if XR_USE_GRAPHICS_API_OPENGL_ES || XR_USE_GRAPHICS_API_OPENGL
  GL(glDeleteRenderbuffers(frameBuffer->swapchain_length, frameBuffer->gl_depth_buffers));
  GL(glDeleteFramebuffers(frameBuffer->swapchain_length, frameBuffer->gl_frame_buffers));
  free(frameBuffer->gl_depth_buffers);
  free(frameBuffer->gl_frame_buffers);
#endif
  OXR(xrDestroySwapchain(frameBuffer->swapchain_color.handle));
  free(frameBuffer->swapchain_image);

  FramebufferClear(frameBuffer);
}

void FramebufferSetCurrent(Framebuffer* framebuffer)
{
#if XR_USE_GRAPHICS_API_OPENGL_ES || XR_USE_GRAPHICS_API_OPENGL
  GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                       framebuffer->gl_frame_buffers[framebuffer->swapchain_index]));
#endif
}

void FramebufferAcquire(Framebuffer* framebuffer)
{
  XrSwapchainImageAcquireInfo acquire_info = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};
  OXR(xrAcquireSwapchainImage(framebuffer->swapchain_color.handle, &acquire_info,
                              &framebuffer->swapchain_index));

  XrSwapchainImageWaitInfo wait_info;
  wait_info.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
  wait_info.next = NULL;
  wait_info.timeout = 1000000; /* timeout in nanoseconds */
  XrResult res = xrWaitSwapchainImage(framebuffer->swapchain_color.handle, &wait_info);
  int i = 0;
  while ((res != XR_SUCCESS) && (i < 10))
  {
    res = xrWaitSwapchainImage(framebuffer->swapchain_color.handle, &wait_info);
    i++;
    DEBUG_LOG_FMT(VR, "Retry xrWaitSwapchainImage {} times due XR_TIMEOUT_EXPIRED (duration {} ms",
                  i, wait_info.timeout * (1E-9));
  }

  framebuffer->acquired = res == XR_SUCCESS;
  FramebufferSetCurrent(framebuffer);

#if XR_USE_GRAPHICS_API_OPENGL_ES || XR_USE_GRAPHICS_API_OPENGL
  GL(glEnable(GL_SCISSOR_TEST));
  GL(glViewport(0, 0, framebuffer->width, framebuffer->height));
  GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
  GL(glScissor(0, 0, framebuffer->width, framebuffer->height));
  GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  GL(glScissor(0, 0, 0, 0));
  GL(glDisable(GL_SCISSOR_TEST));
#endif
}

void FramebufferRelease(Framebuffer* framebuffer)
{
  if (framebuffer->acquired)
  {
    // Clear the alpha channel, other way OpenXR would not transfer the framebuffer fully
#if XR_USE_GRAPHICS_API_OPENGL_ES || XR_USE_GRAPHICS_API_OPENGL
    GL(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE));
    GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT));
    GL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
#endif

    XrSwapchainImageReleaseInfo release_info = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
    OXR(xrReleaseSwapchainImage(framebuffer->swapchain_color.handle, &release_info));
    framebuffer->acquired = false;
  }
}

/*
================================================================================

ovrRenderer

================================================================================
*/

void DisplayClear(Display* renderer)
{
  for (int i = 0; i < MaxNumEyes; i++)
  {
    FramebufferClear(&renderer->framebuffer[i]);
  }
}

void DisplayCreate(XrSession session, Display* display, int width, int height, bool multiview)
{
  display->multiview = multiview;
  int instances = display->multiview ? 1 : MaxNumEyes;
  for (int i = 0; i < instances; i++)
  {
#if XR_USE_GRAPHICS_API_OPENGL_ES
    FramebufferCreateGLES(session, &display->framebuffer[i], width, height, multiview);
#elif XR_USE_GRAPHICS_API_OPENGL
    // TODO
#endif
  }
}

void DisplayDestroy(Display* display)
{
  int instances = display->multiview ? 1 : MaxNumEyes;
  for (int i = 0; i < instances; i++)
  {
    FramebufferDestroy(&display->framebuffer[i]);
  }
}

void DisplayMouseCursor(int x, int y, int sx, int sy)
{
#if XR_USE_GRAPHICS_API_OPENGL_ES || XR_USE_GRAPHICS_API_OPENGL
  GL(glEnable(GL_SCISSOR_TEST));
  GL(glScissor(x, y, sx, sy));
  GL(glViewport(x, y, sx, sy));
  GL(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));
  GL(glClear(GL_COLOR_BUFFER_BIT));
  GL(glDisable(GL_SCISSOR_TEST));
#endif
}

#ifdef ANDROID
void DisplaySetFoveation(XrInstance* instance, XrSession* session, Display* display,
                         XrFoveationLevelFB level, float offset, XrFoveationDynamicFB dynamic)
{
  PFN_xrCreateFoveationProfileFB pfnCreateFoveationProfileFB;
  OXR(xrGetInstanceProcAddr(*instance, "xrCreateFoveationProfileFB",
                            (PFN_xrVoidFunction*)(&pfnCreateFoveationProfileFB)));

  PFN_xrDestroyFoveationProfileFB pfnDestroyFoveationProfileFB;
  OXR(xrGetInstanceProcAddr(*instance, "xrDestroyFoveationProfileFB",
                            (PFN_xrVoidFunction*)(&pfnDestroyFoveationProfileFB)));

  PFN_xrUpdateSwapchainFB pfnUpdateSwapchainFB;
  OXR(xrGetInstanceProcAddr(*instance, "xrUpdateSwapchainFB",
                            (PFN_xrVoidFunction*)(&pfnUpdateSwapchainFB)));

  int instances = display->multiview ? 1 : MaxNumEyes;
  for (int eye = 0; eye < instances; eye++)
  {
    XrFoveationLevelProfileCreateInfoFB level_profile_info;
    memset(&level_profile_info, 0, sizeof(level_profile_info));
    level_profile_info.type = XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB;
    level_profile_info.level = level;
    level_profile_info.verticalOffset = offset;
    level_profile_info.dynamic = dynamic;

    XrFoveationProfileCreateInfoFB profile_info;
    memset(&profile_info, 0, sizeof(profile_info));
    profile_info.type = XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB;
    profile_info.next = &level_profile_info;

    XrFoveationProfileFB foveation_profile;
    pfnCreateFoveationProfileFB(*session, &profile_info, &foveation_profile);

    XrSwapchainStateFoveationFB foveation_update_state;
    memset(&foveation_update_state, 0, sizeof(foveation_update_state));
    foveation_update_state.type = XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB;
    foveation_update_state.profile = foveation_profile;

    pfnUpdateSwapchainFB(display->framebuffer[eye].swapchain_color.handle,
                         (XrSwapchainStateBaseHeaderFB*)(&foveation_update_state));

    pfnDestroyFoveationProfileFB(foveation_profile);
  }
}
#endif

/*
================================================================================

ovrApp

================================================================================
*/

void AppClear(App* app)
{
  app->session_focused = false;
  app->instance = XR_NULL_HANDLE;
  app->session = XR_NULL_HANDLE;
  memset(&app->viewport_config, 0, sizeof(XrViewConfigurationProperties));
  memset(&app->view_config, 0, MaxNumEyes * sizeof(XrViewConfigurationView));
  app->system_id = XR_NULL_SYSTEM_ID;
  app->head_space = XR_NULL_HANDLE;
  app->stage_space = XR_NULL_HANDLE;
  app->fake_space = XR_NULL_HANDLE;
  app->current_space = XR_NULL_HANDLE;
  app->session_active = false;
  app->swap_interval = 1;
  memset(app->layers, 0, sizeof(CompositorLayer) * MaxLayerCount);
  app->layer_count = 0;
  app->main_thread_id = 0;
  app->render_thread_id = 0;

  DisplayClear(&app->renderer);
}

void AppDestroy(App* app)
{
  AppClear(app);
}

void AppHandleSessionStateChanges(App* app, XrSessionState state)
{
  if (state == XR_SESSION_STATE_READY)
  {
    assert(app->session_active == false);

    XrSessionBeginInfo session_begin_info;
    memset(&session_begin_info, 0, sizeof(session_begin_info));
    session_begin_info.type = XR_TYPE_SESSION_BEGIN_INFO;
    session_begin_info.next = NULL;
    session_begin_info.primaryViewConfigurationType = app->viewport_config.viewConfigurationType;

    XrResult result;
    OXR(result = xrBeginSession(app->session, &session_begin_info));
    app->session_active = (result == XR_SUCCESS);
    DEBUG_LOG_FMT(VR, "Session active = {}", app->session_active);

#ifdef ANDROID
    if (app->session_active && GetPlatformFlag(PLATFORM_EXTENSION_PERFORMANCE))
    {
      XrPerfSettingsLevelEXT cpu_performance_level = XR_PERF_SETTINGS_LEVEL_PERFORMANCE_MAX_EXT;
      XrPerfSettingsLevelEXT gpu_performance_level = XR_PERF_SETTINGS_LEVEL_PERFORMANCE_MAX_EXT;

      PFN_xrPerfSettingsSetPerformanceLevelEXT pfnPerfSettingsSetPerformanceLevelEXT = NULL;
      OXR(xrGetInstanceProcAddr(app->instance, "xrPerfSettingsSetPerformanceLevelEXT",
                                (PFN_xrVoidFunction*)(&pfnPerfSettingsSetPerformanceLevelEXT)));

      OXR(pfnPerfSettingsSetPerformanceLevelEXT(app->session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT,
                                                cpu_performance_level));
      OXR(pfnPerfSettingsSetPerformanceLevelEXT(app->session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT,
                                                gpu_performance_level));

      PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR = NULL;
      OXR(xrGetInstanceProcAddr(app->instance, "xrSetAndroidApplicationThreadKHR",
                                (PFN_xrVoidFunction*)(&pfnSetAndroidApplicationThreadKHR)));

      OXR(pfnSetAndroidApplicationThreadKHR(
          app->session, XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR, app->main_thread_id));
      OXR(pfnSetAndroidApplicationThreadKHR(app->session, XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR,
                                            app->render_thread_id));
    }
#endif
  }
  else if (state == XR_SESSION_STATE_STOPPING)
  {
    assert(app->session_active);

    OXR(xrEndSession(app->session));
    app->session_active = false;
  }
}

bool HandleXrEvents(App* app)
{
  XrEventDataBuffer event_data_bufer = {};
  bool recenter = false;

  // Poll for events
  for (;;)
  {
    XrEventDataBaseHeader* base_event_handler = (XrEventDataBaseHeader*)(&event_data_bufer);
    base_event_handler->type = XR_TYPE_EVENT_DATA_BUFFER;
    base_event_handler->next = NULL;
    XrResult r;
    OXR(r = xrPollEvent(app->instance, &event_data_bufer));
    if (r != XR_SUCCESS)
    {
      break;
    }

    switch (base_event_handler->type)
    {
    case XR_TYPE_EVENT_DATA_EVENTS_LOST:
      DEBUG_LOG_FMT(VR, "xrPollEvent: received XR_TYPE_EVENT_DATA_EVENTS_LOST");
      break;
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
    {
      const XrEventDataInstanceLossPending* instance_loss_pending_event =
          (XrEventDataInstanceLossPending*)(base_event_handler);
      DEBUG_LOG_FMT(VR, "xrPollEvent: received XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: time {}",
                    FromXrTime(instance_loss_pending_event->lossTime));
    }
    break;
    case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
      DEBUG_LOG_FMT(VR, "xrPollEvent: received XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED");
      break;
    case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT:
    {
    }
    break;
    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
    {
      recenter = true;
    }
    break;
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
    {
      const XrEventDataSessionStateChanged* session_state_changed_event =
          (XrEventDataSessionStateChanged*)(base_event_handler);
      switch (session_state_changed_event->state)
      {
      case XR_SESSION_STATE_FOCUSED:
        app->session_focused = true;
        break;
      case XR_SESSION_STATE_VISIBLE:
        app->session_focused = false;
        break;
      case XR_SESSION_STATE_READY:
      case XR_SESSION_STATE_STOPPING:
        AppHandleSessionStateChanges(app, session_state_changed_event->state);
        break;
      default:
        break;
      }
    }
    break;
    default:
      DEBUG_LOG_FMT(VR, "xrPollEvent: Unknown event");
      break;
    }
  }
  return recenter;
}
}  // namespace Common::VR
