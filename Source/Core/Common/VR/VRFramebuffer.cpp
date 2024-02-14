// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/VR/VRBase.h"

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

namespace Common::VR
{
bool Framebuffer::Create(XrSession session, int width, int height, bool multiview)
{
#if XR_USE_GRAPHICS_API_OPENGL_ES
  return CreateGL(session, width, height, multiview);
#else
  return false;
#endif
}

void Framebuffer::Destroy()
{
#if XR_USE_GRAPHICS_API_OPENGL_ES
  GL(glDeleteRenderbuffers(swapchain_length, gl_depth_buffers));
  GL(glDeleteFramebuffers(swapchain_length, gl_frame_buffers));
  free(gl_depth_buffers);
  free(gl_frame_buffers);
#endif
  OXR(xrDestroySwapchain(handle));
  free(swapchain_image);
}

void Framebuffer::Acquire()
{
  XrSwapchainImageAcquireInfo acquire_info = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};
  OXR(xrAcquireSwapchainImage(handle, &acquire_info, &swapchain_index));

  XrSwapchainImageWaitInfo wait_info;
  wait_info.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
  wait_info.next = NULL;
  wait_info.timeout = 1000000; /* timeout in nanoseconds */
  XrResult res = xrWaitSwapchainImage(handle, &wait_info);
  int i = 0;
  while ((res != XR_SUCCESS) && (i < 10))
  {
    res = xrWaitSwapchainImage(handle, &wait_info);
    i++;
    DEBUG_LOG_FMT(VR, "Retry xrWaitSwapchainImage {} times due XR_TIMEOUT_EXPIRED (duration {} ms",
                  i, wait_info.timeout * (1E-9));
  }

  acquired = res == XR_SUCCESS;
  SetCurrent();

#if XR_USE_GRAPHICS_API_OPENGL_ES
  GL(glEnable(GL_SCISSOR_TEST));
  GL(glViewport(0, 0, width, height));
  GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
  GL(glScissor(0, 0, width, height));
  GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  GL(glScissor(0, 0, 0, 0));
  GL(glDisable(GL_SCISSOR_TEST));
#endif
}

void Framebuffer::Release()
{
  if (acquired)
  {
    // Clear the alpha channel, other way OpenXR would not transfer the framebuffer fully
#if XR_USE_GRAPHICS_API_OPENGL_ES
    GL(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE));
    GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT));
    GL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
#endif

    XrSwapchainImageReleaseInfo release_info = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
    OXR(xrReleaseSwapchainImage(handle, &release_info));
    acquired = false;
  }
}

void Framebuffer::SetCurrent()
{
#if XR_USE_GRAPHICS_API_OPENGL_ES
  GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl_frame_buffers[swapchain_index]));
#endif
}

#if XR_USE_GRAPHICS_API_OPENGL_ES
bool Framebuffer::CreateGL(XrSession session, int width, int height, bool multiview)
{
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

  this->width = swapchain_info.width;
  this->height = swapchain_info.height;

  // Create the color swapchain.
  swapchain_info.format = GL_SRGB8_ALPHA8;
  swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
  OXR(xrCreateSwapchain(session, &swapchain_info, &handle));
  OXR(xrEnumerateSwapchainImages(handle, 0, &swapchain_length, NULL));
  swapchain_image = malloc(swapchain_length * sizeof(XrSwapchainImageOpenGLESKHR));

  // Populate the swapchain image array.
  for (uint32_t i = 0; i < swapchain_length; i++)
  {
    XrSwapchainImageOpenGLESKHR* swapchain = (XrSwapchainImageOpenGLESKHR*)swapchain_image;
    swapchain[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
    swapchain[i].next = NULL;
  }
  OXR(xrEnumerateSwapchainImages(handle, swapchain_length, &swapchain_length,
                                 (XrSwapchainImageBaseHeader*)swapchain_image));

  gl_depth_buffers = (GLuint*)malloc(  swapchain_length * sizeof(GLuint));
  gl_frame_buffers = (GLuint*)malloc(  swapchain_length * sizeof(GLuint));
  for (uint32_t i = 0; i <   swapchain_length; i++)
  {
    // Create color texture.
    GLuint color_texture = ((XrSwapchainImageOpenGLESKHR*)  swapchain_image)[i].image;
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
      GL(glGenTextures(1, &  gl_depth_buffers[i]));
      GL(glBindTexture(GL_TEXTURE_2D_ARRAY,   gl_depth_buffers[i]));
      GL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH24_STENCIL8, width, height, 2));
      GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));
    }
    else
    {
      GL(glGenRenderbuffers(1, &  gl_depth_buffers[i]));
      GL(glBindRenderbuffer(GL_RENDERBUFFER,   gl_depth_buffers[i]));
      GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height));
      GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
    }

    // Create the frame buffer.
    GL(glGenFramebuffers(1, &  gl_frame_buffers[i]));
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER,   gl_frame_buffers[i]));
    if (multiview)
    {
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                            gl_depth_buffers[i], 0, 0, 2));
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                            gl_depth_buffers[i], 0, 0, 2));
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color_texture,
                                          0, 0, 2));
    }
    else
    {
      GL(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                     gl_depth_buffers[i]));
      GL(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                     gl_depth_buffers[i]));
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
}  // namespace Common::VR
