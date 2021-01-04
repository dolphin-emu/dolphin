// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <gbm.h>
#include <libdrm/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "Common/GL/GLContext.h"

struct EGLContextData
{
  EGLContext ctx;
  EGLSurface surf;
  EGLDisplay dpy;
  EGLConfig config;
  int interval;

  unsigned major;
  unsigned minor;
};

using EGLAcceptConfigCB = bool (*)(void* display_data, EGLDisplay dpy, EGLConfig config);
using DrmFlipHandler = void (*)(int fd, unsigned frame, unsigned sec, unsigned usec, void* data);
using DrmFbDestroyCB = void (*)(struct gbm_bo* bo, void* data);

class EGLDRM
{
public:
  EGLDRM() = default;

  bool Initialize();
  bool SetVideoMode(unsigned width, unsigned height, bool fullscreen);
  bool QueueFlip();
  EGLint* FillDrmEglAttribs(EGLint* attr);
  void SwapEglBuffers();
  void SwapDrm(int interval, int max_swapchain_images);
  bool CreateEGLSurface(EGLContextData* egl, void* native_window);  // public
  void DestroyResources();

  unsigned GetFbWidth() { return m_fb_width; }
  unsigned GetFbHeight() { return m_fb_height; }
  int GetDrmFd() { return m_drm_fd; }
  struct gbm_surface* GetGbmSurface() { return m_gbm_surface; }
  EGLContextData& GetEGL() { return egl; }

private:
  EGLDisplay GetEGLDisplay(EGLenum platform, void* native);
  bool InitEGLContext(EGLenum platform, void* display_data, EGLint* major, EGLint* minor,
                      EGLint* count, const EGLint* attrib_ptr, EGLAcceptConfigCB cb);
  bool WaitFlip(bool block);
  bool DrmWaitFlip(int timeout);
  struct drm_fb* GetFbFromBo(struct gbm_bo* bo);
  bool CreateEGLContext(EGLContextData* egl, const EGLint* egl_attribs);
  bool SetEGLVideoMode();
  bool GetConnector(int fd);
  bool GetEncoder(int fd);
  void FreeResources();

  EGLContextData egl;

  bool m_waiting_for_flip;
  struct gbm_bo* m_bo = nullptr;
  struct gbm_bo* m_next_bo = nullptr;
  struct gbm_surface* m_gbm_surface = nullptr;
  struct gbm_device* m_gbm_dev = nullptr;

  drmEventContext m_drm_evctx;
  struct pollfd m_drm_fds;

  uint32_t m_fb_width = 0;
  uint32_t m_fb_height = 0;
  uint32_t m_connector_id = 0;

  int m_fd = -1;
  int m_drm_fd = 0;
  uint32_t m_crtc_id = 0;
  drmModeCrtc* m_orig_crtc = nullptr;
  drmModeConnector* m_drm_connector = nullptr;
  drmModeModeInfo* m_drm_mode = nullptr;
  drmModeRes* m_drm_resources = nullptr;
  drmModeEncoder* m_drm_encoder = nullptr;
};

class GLContextEGLDRM : public GLContext
{
public:
  ~GLContextEGLDRM() override;

  bool IsHeadless() const override;

  std::unique_ptr<GLContext> CreateSharedContext() override;

  bool MakeCurrent() override;
  bool ClearCurrent() override;

  void UpdateSurface(void* window_handle) override;

  void Swap() override;
  void SwapInterval(int interval) override;

  void* GetFuncAddress(const std::string& name) override;

  static EGLDRM* GetDrm() { return g_drm; }

protected:
  bool Initialize(const WindowSystemInfo& wsi, bool stereo, bool core) override;

  bool CreateWindowSurface();
  void DestroyWindowSurface();
  void DestroyContext();

  static EGLDRM* g_drm;
  bool m_supports_surfaceless = false;
  EGLContextData* m_egl = nullptr;
  int m_interval = 1;
};
