#include "VRFramebuffer.h"

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

/*
================================================================================

ovrFramebuffer

================================================================================
*/

void ovrFramebuffer_Clear(ovrFramebuffer* frameBuffer)
{
  frameBuffer->Width = 0;
  frameBuffer->Height = 0;
  frameBuffer->TextureSwapChainLength = 0;
  frameBuffer->TextureSwapChainIndex = 0;
  frameBuffer->ColorSwapChain.Handle = XR_NULL_HANDLE;
  frameBuffer->ColorSwapChain.Width = 0;
  frameBuffer->ColorSwapChain.Height = 0;
  frameBuffer->ColorSwapChainImage = NULL;

  frameBuffer->GLDepthBuffers = NULL;
  frameBuffer->GLFrameBuffers = NULL;
  frameBuffer->Acquired = false;
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
    ALOGE("GL error on line %s:%d %s", file, line, GlErrorString(error));
  }
}

#endif

#if XR_USE_GRAPHICS_API_OPENGL_ES

static bool ovrFramebuffer_CreateGLES(XrSession session, ovrFramebuffer* frameBuffer, int width,
                                      int height, bool multiview)
{
  frameBuffer->Width = width;
  frameBuffer->Height = height;

  if (strstr((const char*)glGetString(GL_EXTENSIONS), "GL_OVR_multiview2") == nullptr)
  {
    ALOGE("OpenGL implementation does not support GL_OVR_multiview2 extension.\n");
  }

  typedef void (*PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR)(GLenum, GLenum, GLuint, GLint, GLint,
                                                      GLsizei);
  PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR glFramebufferTextureMultiviewOVR = nullptr;
  glFramebufferTextureMultiviewOVR =
      (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR)eglGetProcAddress("glFramebufferTextureMultiviewOVR");
  if (!glFramebufferTextureMultiviewOVR)
  {
    ALOGE("Can not get proc address for glFramebufferTextureMultiviewOVR.\n");
  }
  XrSwapchainCreateInfo swapChainCreateInfo;
  memset(&swapChainCreateInfo, 0, sizeof(swapChainCreateInfo));
  swapChainCreateInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
  swapChainCreateInfo.sampleCount = 1;
  swapChainCreateInfo.width = width;
  swapChainCreateInfo.height = height;
  swapChainCreateInfo.faceCount = 1;
  swapChainCreateInfo.mipCount = 1;
  swapChainCreateInfo.arraySize = multiview ? 2 : 1;

#ifdef ANDROID
  if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_FOVEATION))
  {
    XrSwapchainCreateInfoFoveationFB swapChainFoveationCreateInfo;
    memset(&swapChainFoveationCreateInfo, 0, sizeof(swapChainFoveationCreateInfo));
    swapChainFoveationCreateInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB;
    swapChainCreateInfo.next = &swapChainFoveationCreateInfo;
  }
#endif

  frameBuffer->ColorSwapChain.Width = swapChainCreateInfo.width;
  frameBuffer->ColorSwapChain.Height = swapChainCreateInfo.height;

  // Create the color swapchain.
  swapChainCreateInfo.format = GL_SRGB8_ALPHA8;
  swapChainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
  OXR(xrCreateSwapchain(session, &swapChainCreateInfo, &frameBuffer->ColorSwapChain.Handle));
  OXR(xrEnumerateSwapchainImages(frameBuffer->ColorSwapChain.Handle, 0,
                                 &frameBuffer->TextureSwapChainLength, NULL));
  frameBuffer->ColorSwapChainImage =
      malloc(frameBuffer->TextureSwapChainLength * sizeof(XrSwapchainImageOpenGLESKHR));

  // Populate the swapchain image array.
  for (uint32_t i = 0; i < frameBuffer->TextureSwapChainLength; i++)
  {
    XrSwapchainImageOpenGLESKHR* swapchain =
        (XrSwapchainImageOpenGLESKHR*)frameBuffer->ColorSwapChainImage;
    swapchain[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
    swapchain[i].next = NULL;
  }
  OXR(xrEnumerateSwapchainImages(frameBuffer->ColorSwapChain.Handle,
                                 frameBuffer->TextureSwapChainLength,
                                 &frameBuffer->TextureSwapChainLength,
                                 (XrSwapchainImageBaseHeader*)frameBuffer->ColorSwapChainImage));

  frameBuffer->GLDepthBuffers =
      (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));
  frameBuffer->GLFrameBuffers =
      (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));
  for (uint32_t i = 0; i < frameBuffer->TextureSwapChainLength; i++)
  {
    // Create color texture.
    GLuint colorTexture = ((XrSwapchainImageOpenGLESKHR*)frameBuffer->ColorSwapChainImage)[i].image;
    GLenum colorTextureTarget = multiview ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
    GL(glBindTexture(colorTextureTarget, colorTexture));
    GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glBindTexture(colorTextureTarget, 0));

    // Create depth buffer.
    if (multiview)
    {
      GL(glGenTextures(1, &frameBuffer->GLDepthBuffers[i]));
      GL(glBindTexture(GL_TEXTURE_2D_ARRAY, frameBuffer->GLDepthBuffers[i]));
      GL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH24_STENCIL8, width, height, 2));
      GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));
    }
    else
    {
      GL(glGenRenderbuffers(1, &frameBuffer->GLDepthBuffers[i]));
      GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->GLDepthBuffers[i]));
      GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height));
      GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
    }

    // Create the frame buffer.
    GL(glGenFramebuffers(1, &frameBuffer->GLFrameBuffers[i]));
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->GLFrameBuffers[i]));
    if (multiview)
    {
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                          frameBuffer->GLDepthBuffers[i], 0, 0, 2));
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          frameBuffer->GLDepthBuffers[i], 0, 0, 2));
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTexture,
                                          0, 0, 2));
    }
    else
    {
      GL(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                   frameBuffer->GLDepthBuffers[i]));
      GL(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                   frameBuffer->GLDepthBuffers[i]));
      GL(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                colorTexture, 0));
    }
    GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
    if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
      ALOGE("Incomplete frame buffer object: %d", renderFramebufferStatus);
      return false;
    }
  }

  return true;
}

#endif

void ovrFramebuffer_Destroy(ovrFramebuffer* frameBuffer)
{
#if XR_USE_GRAPHICS_API_OPENGL_ES || XR_USE_GRAPHICS_API_OPENGL
  GL(glDeleteRenderbuffers(frameBuffer->TextureSwapChainLength, frameBuffer->GLDepthBuffers));
  GL(glDeleteFramebuffers(frameBuffer->TextureSwapChainLength, frameBuffer->GLFrameBuffers));
  free(frameBuffer->GLDepthBuffers);
  free(frameBuffer->GLFrameBuffers);
#endif
  OXR(xrDestroySwapchain(frameBuffer->ColorSwapChain.Handle));
  free(frameBuffer->ColorSwapChainImage);

  ovrFramebuffer_Clear(frameBuffer);
}

void ovrFramebuffer_SetCurrent(ovrFramebuffer* frameBuffer)
{
#if XR_USE_GRAPHICS_API_OPENGL_ES || XR_USE_GRAPHICS_API_OPENGL
  GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                       frameBuffer->GLFrameBuffers[frameBuffer->TextureSwapChainIndex]));
#endif
}

void ovrFramebuffer_Acquire(ovrFramebuffer* frameBuffer)
{
  XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};
  OXR(xrAcquireSwapchainImage(frameBuffer->ColorSwapChain.Handle, &acquireInfo,
                              &frameBuffer->TextureSwapChainIndex));

  XrSwapchainImageWaitInfo waitInfo;
  waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
  waitInfo.next = NULL;
  waitInfo.timeout = 1000000; /* timeout in nanoseconds */
  XrResult res = xrWaitSwapchainImage(frameBuffer->ColorSwapChain.Handle, &waitInfo);
  int i = 0;
  while ((res != XR_SUCCESS) && (i < 10))
  {
    res = xrWaitSwapchainImage(frameBuffer->ColorSwapChain.Handle, &waitInfo);
    i++;
    ALOGV(" Retry xrWaitSwapchainImage %d times due to XR_TIMEOUT_EXPIRED (duration %f micro "
          "seconds)",
          i, waitInfo.timeout * (1E-9));
  }
  frameBuffer->Acquired = res == XR_SUCCESS;

  ovrFramebuffer_SetCurrent(frameBuffer);

#if XR_USE_GRAPHICS_API_OPENGL_ES || XR_USE_GRAPHICS_API_OPENGL
  GL(glEnable(GL_SCISSOR_TEST));
  GL(glViewport(0, 0, frameBuffer->Width, frameBuffer->Height));
  GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
  GL(glScissor(0, 0, frameBuffer->Width, frameBuffer->Height));
  GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  GL(glScissor(0, 0, 0, 0));
  GL(glDisable(GL_SCISSOR_TEST));
#endif
}

void ovrFramebuffer_Release(ovrFramebuffer* frameBuffer)
{
  if (frameBuffer->Acquired)
  {
    // Clear the alpha channel, other way OpenXR would not transfer the framebuffer fully
#if XR_USE_GRAPHICS_API_OPENGL_ES || XR_USE_GRAPHICS_API_OPENGL
    GL(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE));
    GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT));
    GL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
#endif

    XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
    OXR(xrReleaseSwapchainImage(frameBuffer->ColorSwapChain.Handle, &releaseInfo));
    frameBuffer->Acquired = false;
  }
}

/*
================================================================================

ovrRenderer

================================================================================
*/

void ovrRenderer_Clear(ovrRenderer* renderer)
{
  for (int i = 0; i < ovrMaxNumEyes; i++)
  {
    ovrFramebuffer_Clear(&renderer->FrameBuffer[i]);
  }
}

void ovrRenderer_Create(XrSession session, ovrRenderer* renderer, int width, int height,
                        bool multiview)
{
  renderer->Multiview = multiview;
  int instances = renderer->Multiview ? 1 : ovrMaxNumEyes;
  for (int i = 0; i < instances; i++)
  {
#if XR_USE_GRAPHICS_API_OPENGL_ES
    ovrFramebuffer_CreateGLES(session, &renderer->FrameBuffer[i], width, height, multiview);
#elif XR_USE_GRAPHICS_API_OPENGL
    // TODO
#endif
  }
}

void ovrRenderer_Destroy(ovrRenderer* renderer)
{
  int instances = renderer->Multiview ? 1 : ovrMaxNumEyes;
  for (int i = 0; i < instances; i++)
  {
    ovrFramebuffer_Destroy(&renderer->FrameBuffer[i]);
  }
}

void ovrRenderer_MouseCursor(ovrRenderer* renderer, int x, int y, int sx, int sy)
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
void ovrRenderer_SetFoveation(XrInstance* instance, XrSession* session, ovrRenderer* renderer,
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

  int instances = renderer->Multiview ? 1 : ovrMaxNumEyes;
  for (int eye = 0; eye < instances; eye++)
  {
    XrFoveationLevelProfileCreateInfoFB levelProfileCreateInfo;
    memset(&levelProfileCreateInfo, 0, sizeof(levelProfileCreateInfo));
    levelProfileCreateInfo.type = XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB;
    levelProfileCreateInfo.level = level;
    levelProfileCreateInfo.verticalOffset = offset;
    levelProfileCreateInfo.dynamic = dynamic;

    XrFoveationProfileCreateInfoFB profileCreateInfo;
    memset(&profileCreateInfo, 0, sizeof(profileCreateInfo));
    profileCreateInfo.type = XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB;
    profileCreateInfo.next = &levelProfileCreateInfo;

    XrFoveationProfileFB foveationProfile;

    pfnCreateFoveationProfileFB(*session, &profileCreateInfo, &foveationProfile);

    XrSwapchainStateFoveationFB foveationUpdateState;
    memset(&foveationUpdateState, 0, sizeof(foveationUpdateState));
    foveationUpdateState.type = XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB;
    foveationUpdateState.profile = foveationProfile;

    pfnUpdateSwapchainFB(renderer->FrameBuffer[eye].ColorSwapChain.Handle,
                         (XrSwapchainStateBaseHeaderFB*)(&foveationUpdateState));

    pfnDestroyFoveationProfileFB(foveationProfile);
  }
}
#endif

/*
================================================================================

ovrApp

================================================================================
*/

void ovrApp_Clear(ovrApp* app)
{
  app->Focused = false;
  app->Instance = XR_NULL_HANDLE;
  app->Session = XR_NULL_HANDLE;
  memset(&app->ViewportConfig, 0, sizeof(XrViewConfigurationProperties));
  memset(&app->ViewConfigurationView, 0, ovrMaxNumEyes * sizeof(XrViewConfigurationView));
  app->SystemId = XR_NULL_SYSTEM_ID;
  app->HeadSpace = XR_NULL_HANDLE;
  app->StageSpace = XR_NULL_HANDLE;
  app->FakeSpace = XR_NULL_HANDLE;
  app->CurrentSpace = XR_NULL_HANDLE;
  app->SessionActive = false;
  app->SwapInterval = 1;
  memset(app->Layers, 0, sizeof(ovrCompositorLayer_Union) * ovrMaxLayerCount);
  app->LayerCount = 0;
  app->MainThreadTid = 0;
  app->RenderThreadTid = 0;

  ovrRenderer_Clear(&app->Renderer);
}

void ovrApp_Destroy(ovrApp* app)
{
  ovrApp_Clear(app);
}

void ovrApp_HandleSessionStateChanges(ovrApp* app, XrSessionState state)
{
  if (state == XR_SESSION_STATE_READY)
  {
    assert(app->SessionActive == false);

    XrSessionBeginInfo sessionBeginInfo;
    memset(&sessionBeginInfo, 0, sizeof(sessionBeginInfo));
    sessionBeginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
    sessionBeginInfo.next = NULL;
    sessionBeginInfo.primaryViewConfigurationType = app->ViewportConfig.viewConfigurationType;

    XrResult result;
    OXR(result = xrBeginSession(app->Session, &sessionBeginInfo));
    app->SessionActive = (result == XR_SUCCESS);
    ALOGV("OpenXR session active = %d", app->SessionActive);

#ifdef ANDROID
    if (app->SessionActive && VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_PERFORMANCE))
    {
      XrPerfSettingsLevelEXT cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_PERFORMANCE_MAX_EXT;
      XrPerfSettingsLevelEXT gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_PERFORMANCE_MAX_EXT;

      PFN_xrPerfSettingsSetPerformanceLevelEXT pfnPerfSettingsSetPerformanceLevelEXT = NULL;
      OXR(xrGetInstanceProcAddr(app->Instance, "xrPerfSettingsSetPerformanceLevelEXT",
                                (PFN_xrVoidFunction*)(&pfnPerfSettingsSetPerformanceLevelEXT)));

      OXR(pfnPerfSettingsSetPerformanceLevelEXT(app->Session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT,
                                                cpuPerfLevel));
      OXR(pfnPerfSettingsSetPerformanceLevelEXT(app->Session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT,
                                                gpuPerfLevel));

      PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR = NULL;
      OXR(xrGetInstanceProcAddr(app->Instance, "xrSetAndroidApplicationThreadKHR",
                                (PFN_xrVoidFunction*)(&pfnSetAndroidApplicationThreadKHR)));

      OXR(pfnSetAndroidApplicationThreadKHR(
          app->Session, XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR, app->MainThreadTid));
      OXR(pfnSetAndroidApplicationThreadKHR(app->Session, XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR,
                                            app->RenderThreadTid));
    }
#endif
  }
  else if (state == XR_SESSION_STATE_STOPPING)
  {
    assert(app->SessionActive);

    OXR(xrEndSession(app->Session));
    app->SessionActive = false;
  }
}

int ovrApp_HandleXrEvents(ovrApp* app)
{
  XrEventDataBuffer eventDataBuffer = {};
  int recenter = 0;

  // Poll for events
  for (;;)
  {
    XrEventDataBaseHeader* baseEventHeader = (XrEventDataBaseHeader*)(&eventDataBuffer);
    baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
    baseEventHeader->next = NULL;
    XrResult r;
    OXR(r = xrPollEvent(app->Instance, &eventDataBuffer));
    if (r != XR_SUCCESS)
    {
      break;
    }

    switch (baseEventHeader->type)
    {
    case XR_TYPE_EVENT_DATA_EVENTS_LOST:
      ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_EVENTS_LOST event");
      break;
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
    {
      const XrEventDataInstanceLossPending* instance_loss_pending_event =
          (XrEventDataInstanceLossPending*)(baseEventHeader);
      ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING event: time %f",
            FromXrTime(instance_loss_pending_event->lossTime));
    }
    break;
    case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
      ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED event");
      break;
    case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT:
    {
    }
    break;
    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
    {
      recenter = 1;
    }
    break;
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
    {
      const XrEventDataSessionStateChanged* session_state_changed_event =
          (XrEventDataSessionStateChanged*)(baseEventHeader);
      switch (session_state_changed_event->state)
      {
      case XR_SESSION_STATE_FOCUSED:
        app->Focused = true;
        break;
      case XR_SESSION_STATE_VISIBLE:
        app->Focused = false;
        break;
      case XR_SESSION_STATE_READY:
      case XR_SESSION_STATE_STOPPING:
        ovrApp_HandleSessionStateChanges(app, session_state_changed_event->state);
        break;
      default:
        break;
      }
    }
    break;
    default:
      ALOGV("xrPollEvent: Unknown event");
      break;
    }
  }
  return recenter;
}
