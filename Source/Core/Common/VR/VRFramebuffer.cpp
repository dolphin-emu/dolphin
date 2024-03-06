// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/VR/VRBase.h"

#if XR_USE_GRAPHICS_API_OPENGL_ES
#include <Common/GL/GLUtil.h>
#endif

namespace Common::VR
{
bool Framebuffer::Create(XrSession session, int width, int height)
{
  m_width = width;
  m_height = height;

#if XR_USE_GRAPHICS_API_OPENGL_ES
  CreateSwapchain(session, m_color_handle, GL_SRGB8_ALPHA8,
                  XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT);
  CreateSwapchain(session, m_depth_handle, GL_DEPTH24_STENCIL8,
                  XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  m_color_images = CreateImages(m_color_handle);
  m_depth_images = CreateImages(m_depth_handle);
  m_frame_buffers = new GLuint[m_length];
  for (uint32_t i = 0; i < m_length; i++)
  {
    GLuint color = ((XrSwapchainImageOpenGLESKHR*)m_color_images)[i].image;
    GLuint depth = ((XrSwapchainImageOpenGLESKHR*)m_depth_images)[i].image;
    if (!GLUtil::CreateFrameBuffer(((GLuint*)m_frame_buffers)[i], color, depth, depth))
    {
      return false;
    }
  }
  return true;
#else
  return false;
#endif
}

void Framebuffer::Destroy()
{
#if XR_USE_GRAPHICS_API_OPENGL_ES
  GLUtil::DestroyFrameBuffers((GLuint*)m_frame_buffers, m_length);
#endif
  OXR(xrDestroySwapchain(m_color_handle));
  OXR(xrDestroySwapchain(m_depth_handle));
  delete[] m_color_images;
  delete[] m_depth_images;
}

void Framebuffer::Acquire()
{
  XrSwapchainImageAcquireInfo acquire_info = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};
  OXR(xrAcquireSwapchainImage(m_color_handle, &acquire_info, &m_index));

  XrSwapchainImageWaitInfo wait_info;
  wait_info.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
  wait_info.next = NULL;
  wait_info.timeout = 1000000; /* timeout in nanoseconds */
  XrResult res = xrWaitSwapchainImage(m_color_handle, &wait_info);
  int i = 0;
  while ((res != XR_SUCCESS) && (i < 10))
  {
    res = xrWaitSwapchainImage(m_color_handle, &wait_info);
    i++;
    DEBUG_LOG_FMT(VR, "Retry xrWaitSwapchainImage {} times due XR_TIMEOUT_EXPIRED (duration {} ms",
                  i, wait_info.timeout * (1E-9));
  }

  m_acquired = res == XR_SUCCESS;
  SetCurrent();

#if XR_USE_GRAPHICS_API_OPENGL_ES
  GLUtil::ClearViewportRect(0, 0, m_width, m_height);
#endif
}

void Framebuffer::Release()
{
  if (m_acquired)
  {
    XrSwapchainImageReleaseInfo release_info = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
    OXR(xrReleaseSwapchainImage(m_color_handle, &release_info));
    m_acquired = false;
  }
}

void Framebuffer::SetCurrent()
{
#if XR_USE_GRAPHICS_API_OPENGL_ES
  GLUtil::BindFrameBuffer(((GLuint*)m_frame_buffers)[m_index]);
#endif
}

XrSwapchainImageBaseHeader* Framebuffer::CreateImages(XrSwapchain handle)
{
  XrSwapchainImageBaseHeader* output;
#if XR_USE_GRAPHICS_API_OPENGL_ES
  output = (XrSwapchainImageBaseHeader*)(new XrSwapchainImageOpenGLESKHR[m_length]);
  for (uint32_t i = 0; i < m_length; i++)
  {
    XrSwapchainImageOpenGLESKHR* swapchain = (XrSwapchainImageOpenGLESKHR*)output;
    swapchain[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
    swapchain[i].next = NULL;
  }
  OXR(xrEnumerateSwapchainImages(handle, m_length, &m_length, output));
#endif
  return output;
}

void Framebuffer::CreateSwapchain(XrSession session, XrSwapchain& handle, int64_t format,
                                  XrFlags64 usage)
{
  XrSwapchainCreateInfo swapchain_info;
  memset(&swapchain_info, 0, sizeof(swapchain_info));
  swapchain_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
  swapchain_info.sampleCount = 1;
  swapchain_info.width = m_width;
  swapchain_info.height = m_height;
  swapchain_info.faceCount = 1;
  swapchain_info.mipCount = 1;
  swapchain_info.arraySize = 1;
  swapchain_info.format = format;
  swapchain_info.usageFlags = usage;
  OXR(xrCreateSwapchain(session, &swapchain_info, &handle));
  OXR(xrEnumerateSwapchainImages(handle, 0, &m_length, NULL));
}
}  // namespace Common::VR
