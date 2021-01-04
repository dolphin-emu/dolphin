// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/GL/GLInterface/EGLDRM.h"
#include "Common/Logging/Log.h"
#include "Core/HW/VideoInterface.h"  //for TargetRefreshRate

#ifndef EGL_CONTEXT_FLAGS_KHR
#define EGL_CONTEXT_FLAGS_KHR 0x30FC
#endif

#ifndef EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR 0x00000001
#endif

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

#ifndef EGL_PLATFORM_GBM_KHR
#define EGL_PLATFORM_GBM_KHR 0x31D7
#endif

#ifndef EGL_KHR_create_context
#define EGL_KHR_create_context 1
#define EGL_CONTEXT_MAJOR_VERSION_KHR 0x3098
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB
#endif /* EGL_KHR_create_context */

#define DRM_EGL_ATTRIBS_BASE                                                                       \
  EGL_SURFACE_TYPE, 0 /*EGL_WINDOW_BIT*/, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,    \
      EGL_ALPHA_SIZE, 0, EGL_DEPTH_SIZE, 0

#ifdef EGL_KHR_create_context
static const EGLint egl_attribs_gles3[] = {
    DRM_EGL_ATTRIBS_BASE,
    EGL_RENDERABLE_TYPE,
    EGL_OPENGL_ES3_BIT_KHR,
    EGL_NONE,
};
#endif

EGLDRM* GLContextEGLDRM::g_drm = nullptr;

struct drm_fb
{
  struct gbm_bo* bo;
  uint32_t fb_id;
};

static void drm_flip_handler(int fd, unsigned frame, unsigned sec, unsigned usec, void* data)
{
  *(bool*)data = false;
}

static bool gbm_choose_xrgb8888_cb(void* display_data, EGLDisplay dpy, EGLConfig config)
{
  EGLint r, g, b, id;
  (void)display_data;

  /* Makes sure we have 8 bit color. */
  if (!eglGetConfigAttrib(dpy, config, EGL_RED_SIZE, &r))
    return false;
  if (!eglGetConfigAttrib(dpy, config, EGL_GREEN_SIZE, &g))
    return false;
  if (!eglGetConfigAttrib(dpy, config, EGL_BLUE_SIZE, &b))
    return false;

  if (r != 8 || g != 8 || b != 8)
    return false;

  if (!eglGetConfigAttrib(dpy, config, EGL_NATIVE_VISUAL_ID, &id))
    return false;

  return id == GBM_FORMAT_XRGB8888;
}

static void drm_fb_destroy_callback(struct gbm_bo* bo, void* data)
{
  struct drm_fb* fb = (struct drm_fb*)data;

  if (fb && fb->fb_id && GLContextEGLDRM::GetDrm())
    drmModeRmFB(GLContextEGLDRM::GetDrm()->GetDrmFd(), fb->fb_id);

  free(fb);
}

static bool check_egl_client_extension(const char* name)
{
  size_t nameLen;
  const char* str = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  /* The EGL implementation doesn't support client extensions at all. */
  if (!str)
    return false;

  nameLen = strlen(name);
  while (*str != '\0')
  {
    /* Use strspn and strcspn to find the start position and length of each
     * token in the extension string. Using strtok could also work, but
     * that would require allocating a copy of the string. */
    size_t len = strcspn(str, " ");
    if (len == nameLen && strncmp(str, name, nameLen) == 0)
      return true;
    str += len;
    str += strspn(str, " ");
  }

  return false;
}

static bool check_egl_display_extension(EGLContextData* egl, const char* name)
{
  size_t nameLen;
  if (!egl || egl->dpy == EGL_NO_DISPLAY)
    return false;

  const char* str = eglQueryString(egl->dpy, EGL_EXTENSIONS);
  /* The EGL implementation doesn't support extensions at all. */
  if (!str)
    return false;

  nameLen = strlen(name);
  while (*str != '\0')
  {
    /* Use strspn and strcspn to find the start position and length of each
     * token in the extension string. Using strtok could also work, but
     * that would require allocating a copy of the string. */
    size_t len = strcspn(str, " ");
    if (len == nameLen && strncmp(str, name, nameLen) == 0)
      return true;
    str += len;
    str += strspn(str, " ");
  }

  return false;
}

static bool check_egl_version(int minMajorVersion, int minMinorVersion)
{
  int count;
  int major, minor;
  const char* str = eglQueryString(EGL_NO_DISPLAY, EGL_VERSION);

  if (!str)
    return false;

  count = sscanf(str, "%d.%d", &major, &minor);
  if (count != 2)
    return false;

  if (major < minMajorVersion)
    return false;

  if (major > minMajorVersion)
    return true;

  if (minor >= minMinorVersion)
    return true;

  return false;
}

struct drm_fb* EGLDRM::GetFbFromBo(struct gbm_bo* gbm_bo)
{
  int ret;
  unsigned width, height, stride, handle;
  struct drm_fb* fb = (struct drm_fb*)calloc(1, sizeof(*fb));

  fb->bo = gbm_bo;

  width = gbm_bo_get_width(gbm_bo);
  height = gbm_bo_get_height(gbm_bo);
  stride = gbm_bo_get_stride(gbm_bo);
  handle = gbm_bo_get_handle(gbm_bo).u32;

  INFO_LOG(VIDEO, "[KMS]: New FB: %ux%u (stride: %u).\n", width, height, stride);

  ret = drmModeAddFB(this->m_drm_fd, width, height, 24, 32, stride, handle, &fb->fb_id);
  if (ret < 0)
    goto error;

  gbm_bo_set_user_data(gbm_bo, fb, drm_fb_destroy_callback);
  return fb;

error:
  INFO_LOG(VIDEO, "[KMS]: Failed to create FB: %s\n", strerror(errno));
  free(fb);
  return nullptr;
}

bool EGLDRM::WaitFlip(bool block)
{
  int timeout = 0;

  if (!this->m_waiting_for_flip)
    return false;

  if (block)
    timeout = -1;

  while (this->m_waiting_for_flip)
  {
    if (!DrmWaitFlip(timeout))
      break;
  }

  if (this->m_waiting_for_flip)
    return true;

  /* Page flip has taken place. */

  /* This buffer is not on-screen anymore. Release it to GBM. */
  gbm_surface_release_buffer(this->m_gbm_surface, this->m_bo);

  /* This buffer is being shown now. */
  this->m_bo = this->m_next_bo;
  return false;
}

bool EGLDRM::QueueFlip()
{
  struct drm_fb* fb = nullptr;

  this->m_next_bo = gbm_surface_lock_front_buffer(this->m_gbm_surface);
  fb = (struct drm_fb*)gbm_bo_get_user_data(this->m_next_bo);

  if (!fb)
    fb = (struct drm_fb*)GetFbFromBo(this->m_next_bo);

  if (drmModePageFlip(this->m_drm_fd, this->m_crtc_id, fb->fb_id, DRM_MODE_PAGE_FLIP_EVENT,
                      &this->m_waiting_for_flip) == 0)
    return true;

  /* Failed to queue page flip. */
  INFO_LOG(VIDEO, "\nFailed to queue page flip\n");
  return false;
}

bool EGLDRM::Initialize()
{
  this->m_fd = -1;

  FreeResources();

  this->m_fd = open("/dev/dri/card0", O_RDWR);
  if (this->m_fd < 0)
  {
    INFO_LOG(VIDEO, "[KMS]: Couldn't open DRM device.\n");
    return false;
  }

  this->m_drm_resources = drmModeGetResources(this->m_fd);
  if (!this->m_drm_resources)
  {
    INFO_LOG(VIDEO, "[KMS]: DRM couldn't get device resources.\n");
    return false;
  }

  if (!GetConnector(this->m_fd))
  {
    INFO_LOG(VIDEO, "[KMS]: DRM GetConnector failed\n");
    return false;
  }

  if (!GetEncoder(this->m_fd))
  {
    INFO_LOG(VIDEO, "[KMS]: DRM GetEncoder failed\n");
    return false;
  }

  this->m_crtc_id = this->m_drm_encoder->crtc_id;
  this->m_connector_id = this->m_drm_connector->connector_id;
  this->m_orig_crtc = drmModeGetCrtc(this->m_fd, this->m_crtc_id);
  if (!this->m_orig_crtc)
    INFO_LOG(VIDEO, "[DRM]: Cannot find original CRTC.\n");

  /* Choose the optimal video mode for get_video_size():
    - the current video mode from the CRTC
    - otherwise pick first connector mode */
  if (this->m_orig_crtc->mode_valid)
  {
    this->m_fb_width = this->m_orig_crtc->mode.hdisplay;
    this->m_fb_height = this->m_orig_crtc->mode.vdisplay;
  }
  else
  {
    this->m_fb_width = this->m_drm_connector->modes[0].hdisplay;
    this->m_fb_height = this->m_drm_connector->modes[0].vdisplay;
  }

  drmSetMaster(this->m_drm_fd);

  this->m_gbm_dev = gbm_create_device(this->m_fd);

  if (!this->m_gbm_dev)
  {
    INFO_LOG(VIDEO, "[KMS]: Couldn't create GBM device.\n");
    return false;
  }

  /* Setup the flip handler. */
  this->m_drm_fds.fd = this->m_fd;
  this->m_drm_fds.events = POLLIN;
  this->m_drm_evctx.version = DRM_EVENT_CONTEXT_VERSION;
  this->m_drm_evctx.page_flip_handler = drm_flip_handler;

  this->m_drm_fd = this->m_fd;

  /* Initialization complete */
  return true;
}

void EGLDRM::DestroyResources()
{
  /* Make sure we acknowledge all page-flips. */
  WaitFlip(true);

  if (this->egl.dpy)
  {
    eglMakeCurrent(this->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (this->egl.ctx != EGL_NO_CONTEXT)
      eglDestroyContext(this->egl.dpy, this->egl.ctx);

    if (this->egl.surf != EGL_NO_SURFACE)
      eglDestroySurface(this->egl.dpy, this->egl.surf);
    eglTerminate(this->egl.dpy);
  }

  /* Be as careful as possible in deinit.
   * If we screw up, any TTY will not restore.
   */

  this->egl.ctx = EGL_NO_CONTEXT;
  this->egl.surf = EGL_NO_SURFACE;
  this->egl.dpy = EGL_NO_DISPLAY;
  this->egl.config = 0;

  FreeResources();

  this->m_drm_mode = nullptr;
  this->m_crtc_id = 0;
  this->m_connector_id = 0;

  this->m_fb_width = 0;
  this->m_fb_height = 0;

  this->m_bo = nullptr;
  this->m_next_bo = nullptr;
}

EGLint* EGLDRM::FillDrmEglAttribs(EGLint* attr)
{
  *attr++ = EGL_CONTEXT_CLIENT_VERSION;
  *attr++ = this->egl.major ? (EGLint)this->egl.major : 2;
#ifdef EGL_KHR_create_context
  if (this->egl.minor > 0)
  {
    *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
    *attr++ = this->egl.minor;
  }
#endif

  *attr = EGL_NONE;
  return attr;
}

void EGLDRM::SwapEglBuffers()
{
  if (egl.dpy != EGL_NO_DISPLAY && egl.surf != EGL_NO_SURFACE)
  {
    eglSwapBuffers(egl.dpy, egl.surf);
  }
}

void EGLDRM::SwapDrm(int interval, int max_swapchain_images)
{
  /* I guess we have to wait for flip to have taken
   * place before another flip can be queued up.
   *
   * If true, we are still waiting for a flip
   * (nonblocking mode, so just drop the frame). */
  if (WaitFlip(interval))
  {
    INFO_LOG(VIDEO, "\nwait flip");
    return;
  }

  this->m_waiting_for_flip = QueueFlip();

  /* Triple-buffered page flips */
  if (max_swapchain_images >= 3 && gbm_surface_has_free_buffers(this->m_gbm_surface))
    return;

  WaitFlip(true);
}

bool EGLDRM::GetConnector(int connector_fd)
{
  unsigned i;
  unsigned monitor_index_count = 0;
  unsigned monitor = 1;

  /* Enumerate all connectors. */

  INFO_LOG(VIDEO, "[DRM]: Found %d connectors.\n", this->m_drm_resources->count_connectors);

  for (i = 0; (int)i < this->m_drm_resources->count_connectors; i++)
  {
    drmModeConnectorPtr conn =
        drmModeGetConnector(connector_fd, this->m_drm_resources->connectors[i]);

    if (conn)
    {
      bool connected = conn->connection == DRM_MODE_CONNECTED;
      INFO_LOG(VIDEO, "[DRM]: Connector %d connected: %s\n", i, connected ? "yes" : "no");
      INFO_LOG(VIDEO, "[DRM]: Connector %d has %d modes.\n", i, conn->count_modes);
      if (connected && conn->count_modes > 0)
      {
        monitor_index_count++;
        INFO_LOG(VIDEO, "[DRM]: Connector %d assigned to monitor index: #%u.\n", i,
                 monitor_index_count);
      }
      drmModeFreeConnector(conn);
    }
  }

  monitor_index_count = 0;

  for (i = 0; (int)i < this->m_drm_resources->count_connectors; i++)
  {
    this->m_drm_connector = drmModeGetConnector(connector_fd, this->m_drm_resources->connectors[i]);

    if (!this->m_drm_connector)
      continue;
    if (this->m_drm_connector->connection == DRM_MODE_CONNECTED &&
        this->m_drm_connector->count_modes > 0)
    {
      monitor_index_count++;
      if (monitor_index_count == monitor)
        break;
    }

    drmModeFreeConnector(this->m_drm_connector);
    this->m_drm_connector = nullptr;
  }

  if (!this->m_drm_connector)
  {
    INFO_LOG(VIDEO, "[DRM]: Couldn't get device connector.\n");
    return false;
  }
  return true;
}

bool EGLDRM::GetEncoder(int encoder_fd)
{
  unsigned i;

  for (i = 0; (int)i < this->m_drm_resources->count_encoders; i++)
  {
    this->m_drm_encoder = drmModeGetEncoder(encoder_fd, this->m_drm_resources->encoders[i]);

    if (!this->m_drm_encoder)
      continue;

    if (this->m_drm_encoder->encoder_id == this->m_drm_connector->encoder_id)
      break;

    drmModeFreeEncoder(this->m_drm_encoder);
    this->m_drm_encoder = nullptr;
  }

  if (!this->m_drm_encoder)
  {
    INFO_LOG(VIDEO, "[DRM]: Couldn't find DRM encoder.\n");
    return false;
  }

  for (i = 0; (int)i < this->m_drm_connector->count_modes; i++)
  {
    INFO_LOG(VIDEO, "[DRM]: Mode %d: (%s) %d x %d, %u Hz\n", i,
             this->m_drm_connector->modes[i].name, this->m_drm_connector->modes[i].hdisplay,
             this->m_drm_connector->modes[i].vdisplay, this->m_drm_connector->modes[i].vrefresh);
  }

  return true;
}

bool EGLDRM::DrmWaitFlip(int timeout)
{
  this->m_drm_fds.revents = 0;

  if (poll(&this->m_drm_fds, 1, timeout) < 0)
    return false;

  if (this->m_drm_fds.revents & (POLLHUP | POLLERR))
    return false;

  if (this->m_drm_fds.revents & POLLIN)
  {
    drmHandleEvent(this->m_drm_fd, &this->m_drm_evctx);
    return true;
  }

  return false;
}

void EGLDRM::FreeResources()
{
  /* Restore original CRTC. */
  if (this->m_orig_crtc)
  {
    drmModeSetCrtc(this->m_drm_fd, this->m_orig_crtc->crtc_id, this->m_orig_crtc->buffer_id,
                   this->m_orig_crtc->x, this->m_orig_crtc->y, &this->m_connector_id, 1,
                   &this->m_orig_crtc->mode);

    drmModeFreeCrtc(this->m_orig_crtc);
    this->m_orig_crtc = nullptr;
  }

  if (this->m_gbm_surface)
    gbm_surface_destroy(this->m_gbm_surface);

  if (this->m_gbm_dev)
    gbm_device_destroy(this->m_gbm_dev);

  if (this->m_drm_encoder)
    drmModeFreeEncoder(this->m_drm_encoder);
  if (this->m_drm_connector)
    drmModeFreeConnector(this->m_drm_connector);
  if (this->m_drm_resources)
    drmModeFreeResources(this->m_drm_resources);

  memset(&this->m_drm_fds, 0, sizeof(struct pollfd));
  memset(&this->m_drm_evctx, 0, sizeof(drmEventContext));

  this->m_drm_encoder = nullptr;
  this->m_drm_connector = nullptr;
  this->m_drm_resources = nullptr;

  if (this->m_fd >= 0)
  {
    if (this->m_drm_fd >= 0)
    {
      drmDropMaster(this->m_drm_fd);
      close(this->m_fd);
    }
  }

  this->m_gbm_surface = nullptr;
  this->m_gbm_dev = nullptr;
  this->m_drm_fd = -1;
}

EGLDisplay EGLDRM::GetEGLDisplay(EGLenum platform, void* native)
{
  if (platform != EGL_NONE)
  {
    /* If the client library supports at least EGL 1.5, then we can call
     * eglGetPlatformDisplay. Otherwise, see if eglGetPlatformDisplayEXT
     * is available. */
#if defined(EGL_VERSION_1_5)
    if (check_egl_version(1, 5))
    {
      typedef EGLDisplay(EGLAPIENTRY * pfn_eglGetPlatformDisplay)(
          EGLenum platform, void* native_display, const EGLAttrib* attrib_list);
      pfn_eglGetPlatformDisplay ptr_eglGetPlatformDisplay;

      INFO_LOG(VIDEO, "[EGL] Found EGL client version >= 1.5, trying eglGetPlatformDisplay\n");
      ptr_eglGetPlatformDisplay =
          (pfn_eglGetPlatformDisplay)eglGetProcAddress("eglGetPlatformDisplay");

      if (ptr_eglGetPlatformDisplay)
      {
        EGLDisplay dpy = ptr_eglGetPlatformDisplay(platform, native, nullptr);
        if (dpy != EGL_NO_DISPLAY)
          return dpy;
      }
    }
#endif /* defined(EGL_VERSION_1_5) */

#if defined(EGL_EXT_platform_base)
    if (check_egl_client_extension("EGL_EXT_platform_base"))
    {
      PFNEGLGETPLATFORMDISPLAYEXTPROC ptr_eglGetPlatformDisplayEXT;

      INFO_LOG(VIDEO, "[EGL] Found EGL_EXT_platform_base, trying eglGetPlatformDisplayEXT\n");
      ptr_eglGetPlatformDisplayEXT =
          (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

      if (ptr_eglGetPlatformDisplayEXT)
      {
        EGLDisplay dpy = ptr_eglGetPlatformDisplayEXT(platform, native, nullptr);
        if (dpy != EGL_NO_DISPLAY)
          return dpy;
      }
    }
#endif /* defined(EGL_EXT_platform_base) */
  }

  /* Either the caller didn't provide a platform type, or the EGL
   * implementation doesn't support eglGetPlatformDisplay. In this case, try
   * eglGetDisplay and hope for the best. */
  INFO_LOG(VIDEO, "[EGL] Falling back to eglGetDisplay\n");
  return eglGetDisplay((EGLNativeDisplayType)native);
}

bool EGLDRM::InitEGLContext(EGLenum platform, void* display_data, EGLint* major, EGLint* minor,
                            EGLint* count, const EGLint* attrib_ptr, EGLAcceptConfigCB cb)
{
  EGLint i;
  EGLint matched = 0;
  EGLConfig* configs = nullptr;
  EGLDisplay dpy = GetEGLDisplay(platform, display_data);

  if (dpy == EGL_NO_DISPLAY)
  {
    INFO_LOG(VIDEO, "[EGL]: Couldn't get EGL display.\n");
    return false;
  }

  egl.dpy = dpy;

  if (!eglInitialize(egl.dpy, major, minor))
    return false;

  INFO_LOG(VIDEO, "[EGL]: EGL version: %d.%d\n", *major, *minor);

  if (!eglGetConfigs(egl.dpy, nullptr, 0, count) || *count < 1)
  {
    INFO_LOG(VIDEO, "[EGL]: No configs to choose from.\n");
    return false;
  }

  configs = (EGLConfig*)malloc(*count * sizeof(*configs));
  if (!configs)
    return false;

  if (!eglChooseConfig(egl.dpy, attrib_ptr, configs, *count, &matched) || !matched)
  {
    INFO_LOG(VIDEO, "[EGL]: No EGL configs with appropriate attributes.\n");
    return false;
  }

  for (i = 0; i < *count; i++)
  {
    if (!cb || cb(display_data, egl.dpy, configs[i]))
    {
      egl.config = configs[i];
      break;
    }
  }

  free(configs);

  if (i == *count)
  {
    INFO_LOG(VIDEO, "[EGL]: No EGL config found which satifies requirements.\n");
    return false;
  }

  return true;
}

bool EGLDRM::CreateEGLContext(EGLContextData* _egl, const EGLint* egl_attribs)
{
  EGLContext ctx = eglCreateContext(_egl->dpy, _egl->config, EGL_NO_CONTEXT, egl_attribs);

  if (ctx == EGL_NO_CONTEXT)
    return false;

  _egl->ctx = ctx;

  return true;
}

bool EGLDRM::CreateEGLSurface(EGLContextData* _egl, void* native_window)
{
  EGLint window_attribs[] = {
      EGL_RENDER_BUFFER,
      EGL_BACK_BUFFER,
      EGL_NONE,
  };

  _egl->surf = eglCreateWindowSurface(_egl->dpy, _egl->config, (NativeWindowType)native_window,
                                      window_attribs);

  if (_egl->surf == EGL_NO_SURFACE)
    return false;

  /* Connect the context to the surface. */
  if (!eglMakeCurrent(_egl->dpy, _egl->surf, _egl->surf, _egl->ctx))
    return false;

  INFO_LOG(VIDEO, "[EGL]: Current context: %p.\n", (void*)eglGetCurrentContext());

  return true;
}

bool EGLDRM::SetEGLVideoMode()
{
  const EGLint* attrib_ptr = nullptr;
  EGLint major;
  EGLint minor;
  EGLint n;
  EGLint egl_attribs[16];
  EGLint* egl_attribs_ptr = nullptr;
  EGLint* attr = nullptr;

  attrib_ptr = egl_attribs_gles3;

  if (!InitEGLContext(EGL_PLATFORM_GBM_KHR, (EGLNativeDisplayType)this->m_gbm_dev, &major, &minor,
                      &n, attrib_ptr, gbm_choose_xrgb8888_cb))
  {
    INFO_LOG(VIDEO, "\n[EGL] Cannot init context error 0x%x", eglGetError());
    goto error;
  }
  attr = FillDrmEglAttribs(egl_attribs);
  egl_attribs_ptr = &egl_attribs[0];

  if (!CreateEGLContext(&this->egl, (attr != egl_attribs_ptr) ? egl_attribs_ptr : nullptr))
  {
    INFO_LOG(VIDEO, "\n[EGL] Cannot create context error 0x%x", eglGetError());
    goto error;
  }

  if (!CreateEGLSurface(&this->egl, (EGLNativeWindowType)this->m_gbm_surface))
  {
    INFO_LOG(VIDEO, "\n[EGL] Cannot create context error 0x%x", eglGetError());
    return false;
  }

  SwapEglBuffers();

  return true;

error:
  return false;
}

bool EGLDRM::SetVideoMode(unsigned width, unsigned height, bool fullscreen)
{
  int i, ret = 0;
  struct drm_fb* fb = nullptr;
  float video_refresh_rate = (float)VideoInterface::GetTargetRefreshRate();

  /* Find desired video mode, and use that.
   * If not fullscreen, we get desired windowed size,
   * which is not appropriate. */
  if ((width == 0 && height == 0) || !fullscreen)
    this->m_drm_mode = &this->m_drm_connector->modes[0];
  else
  {
    /* Try to match refresh_rate as closely as possible.
     *
     * Lower resolutions tend to have multiple supported
     * refresh rates as well.
     */
    float minimum_fps_diff = 0.0f;

    /* Find best match. */
    for (i = 0; i < this->m_drm_connector->count_modes; i++)
    {
      float diff;
      if (width != this->m_drm_connector->modes[i].hdisplay ||
          height != this->m_drm_connector->modes[i].vdisplay)
        continue;

      diff = fabsf(this->m_drm_connector->modes[i].vrefresh - video_refresh_rate);

      if (!this->m_drm_mode || diff < minimum_fps_diff)
      {
        this->m_drm_mode = &this->m_drm_connector->modes[i];
        minimum_fps_diff = diff;
      }
    }
  }

  if (!this->m_drm_mode)
  {
    INFO_LOG(VIDEO, "[KMS/EGL]: Did not find suitable video mode for %u x %u.\n", width, height);
    goto error;
  }

  this->m_fb_width = this->m_drm_mode->hdisplay;
  this->m_fb_height = this->m_drm_mode->vdisplay;

  /* Create GBM surface. */
  this->m_gbm_surface =
      gbm_surface_create(this->m_gbm_dev, this->m_fb_width, this->m_fb_height, GBM_FORMAT_XRGB8888,
                         GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

  if (!this->m_gbm_surface)
  {
    INFO_LOG(VIDEO, "[KMS/EGL]: Couldn't create GBM surface.\n");
    goto error;
  }

  if (!SetEGLVideoMode())
  {
    INFO_LOG(VIDEO, "[KMS/EGL]: Couldn't set EGL video mode.\n");
    goto error;
  }

  this->m_bo = gbm_surface_lock_front_buffer(this->m_gbm_surface);

  fb = (struct drm_fb*)gbm_bo_get_user_data(this->m_bo);

  if (!fb)
    fb = GetFbFromBo(this->m_bo);

  ret = drmModeSetCrtc(this->m_drm_fd, this->m_crtc_id, fb->fb_id, 0, 0, &this->m_connector_id, 1,
                       this->m_drm_mode);
  if (ret < 0)
  {
    INFO_LOG(VIDEO, "[KMS/EGL]: drmModeSetCrtc failed\n");
    goto error;
  }
  return true;

error:
  DestroyResources();

  // TODO destroy ??

  return false;
}

GLContextEGLDRM::~GLContextEGLDRM()
{
  DestroyWindowSurface();
  DestroyContext();

  if (g_drm)
  {
    g_drm->DestroyResources();
    delete g_drm;
  }
  g_drm = nullptr;
}

bool GLContextEGLDRM::IsHeadless() const
{
  return false;
}

void GLContextEGLDRM::Swap()
{
  unsigned max_swapchain_images = 2;  // double-buffering
  g_drm->SwapEglBuffers();
  g_drm->SwapDrm(m_interval, max_swapchain_images);
}

void GLContextEGLDRM::SwapInterval(int interval)
{
  m_interval = interval;
  if (interval > 1)
    INFO_LOG(VIDEO,
             "[KMS]: Swap intervals > 1 currently not supported. Will use swap interval of 1.\n");

  /* Can be called before initialization.
   * Some contexts require that swap interval
   * is known at startup time.
   */
  m_egl->interval = interval;

  if (m_egl->dpy == EGL_NO_DISPLAY)
    return;
  if (!eglGetCurrentContext())
    return;

  INFO_LOG(VIDEO, "[EGL]: eglSwapInterval(%u)\n", interval);
  if (!eglSwapInterval(m_egl->dpy, interval))
  {
    INFO_LOG(VIDEO, "[EGL]: eglSwapInterval() failed 0x%x.\n", eglGetError());
  }
}

void* GLContextEGLDRM::GetFuncAddress(const std::string& name)
{
  return (void*)eglGetProcAddress(name.c_str());
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool GLContextEGLDRM::Initialize(const WindowSystemInfo& wsi, bool stereo, bool core)
{
  if (g_drm == nullptr)
    g_drm = new EGLDRM();
  if (!g_drm)
    return false;
  g_drm->Initialize();

  eglBindAPI(EGL_OPENGL_ES_API);

  // Use current video mode, do not switch
  g_drm->SetVideoMode(0, 0, false);
  m_backbuffer_width = g_drm->GetFbWidth();
  m_backbuffer_height = g_drm->GetFbHeight();

  m_egl = &g_drm->GetEGL();
  m_opengl_mode = Mode::OpenGLES;
  m_supports_surfaceless = check_egl_display_extension(m_egl, "EGL_KHR_surfaceless_context");

  eglBindAPI(EGL_OPENGL_ES_API);

  return MakeCurrent();
}

std::unique_ptr<GLContext> GLContextEGLDRM::CreateSharedContext()
{
  std::unique_ptr<GLContextEGLDRM> new_context = std::make_unique<GLContextEGLDRM>();
  new_context->m_egl = (EGLContextData*)malloc(sizeof(EGLContextData));
  memcpy(new_context->m_egl, m_egl, sizeof(EGLContextData));

  eglBindAPI(EGL_OPENGL_ES_API);
  EGLint egl_attribs[16];
  EGLint* egl_attribs_ptr = nullptr;
  g_drm->FillDrmEglAttribs(egl_attribs);
  egl_attribs_ptr = &egl_attribs[0];
  new_context->m_egl->ctx =
      eglCreateContext(m_egl->dpy, m_egl->config, m_egl->ctx, egl_attribs_ptr);
  if (!new_context->m_egl->ctx)
  {
    ERROR_LOG(VIDEO, "\nError: eglCreateContext failed 0x%x\n", eglGetError());
    return nullptr;
  }
  eglBindAPI(EGL_OPENGL_ES_API);
  new_context->m_opengl_mode = Mode::OpenGLES;
  new_context->m_supports_surfaceless = m_supports_surfaceless;
  new_context->m_is_shared = true;
  if (!new_context->CreateWindowSurface())
  {
    ERROR_LOG(VIDEO, "\nError: CreateWindowSurface failed 0x%x\n", eglGetError());
    return nullptr;
  }
  return new_context;
}

bool GLContextEGLDRM::CreateWindowSurface()
{
  EGLint attrib_list[] = {
      EGL_NONE,
  };
  if (m_supports_surfaceless)
  {
    m_egl->surf = EGL_NO_SURFACE;
    INFO_LOG(VIDEO, "\nCreated surfaceless EGL shared context\n");
    return true;
  }

  if (!IsHeadless())
  {
    if (!g_drm->CreateEGLSurface(m_egl, (EGLNativeWindowType)g_drm->GetGbmSurface()))
    {
      INFO_LOG(VIDEO, "\negl_create_surface failed (error 0x%x), trying pbuffer instead...\n",
               eglGetError());
      goto pbuffer;
    }
    // Get dimensions from the surface.
    EGLint surface_width = 1, surface_height = 1;
    if (!eglQuerySurface(m_egl->dpy, m_egl->surf, EGL_WIDTH, &surface_width) ||
        !eglQuerySurface(m_egl->dpy, m_egl->surf, EGL_HEIGHT, &surface_height))
    {
      INFO_LOG(VIDEO,
               "Failed to get surface dimensions via eglQuerySurface. Size may be incorrect.");
    }
    m_backbuffer_width = static_cast<int>(surface_width);
    m_backbuffer_height = static_cast<int>(surface_height);
    return true;
  }

pbuffer:
  m_egl->surf = eglCreatePbufferSurface(m_egl->dpy, m_egl->config, attrib_list);
  if (!m_egl->surf)
  {
    INFO_LOG(VIDEO, "\nError: eglCreatePbufferSurface failed 0x%x\n", eglGetError());
    return false;
  }
  return true;
}

void GLContextEGLDRM::DestroyWindowSurface()
{
  if (m_egl->surf == EGL_NO_SURFACE)
    return;

  if (eglGetCurrentSurface(EGL_DRAW) == m_egl->surf)
    eglMakeCurrent(m_egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (!eglDestroySurface(m_egl->dpy, m_egl->surf))
    INFO_LOG(VIDEO, "\nCould not destroy window surface.");
  m_egl->surf = EGL_NO_SURFACE;
}

bool GLContextEGLDRM::MakeCurrent()
{
  return eglMakeCurrent(m_egl->dpy, g_drm->GetEGL().surf, g_drm->GetEGL().surf, m_egl->ctx);
}

void GLContextEGLDRM::UpdateSurface(void* window_handle)
{
  ClearCurrent();
  DestroyWindowSurface();
  CreateWindowSurface();
  MakeCurrent();
}

bool GLContextEGLDRM::ClearCurrent()
{
  return eglMakeCurrent(m_egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

// Close backend
void GLContextEGLDRM::DestroyContext()
{
  if (!m_egl->ctx)
    return;

  if (eglGetCurrentContext() == m_egl->ctx)
    eglMakeCurrent(m_egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (!eglDestroyContext(m_egl->dpy, m_egl->ctx))
    INFO_LOG(VIDEO, "\nCould not destroy drawing context.");
  if (!m_is_shared && !eglTerminate(m_egl->dpy))
    INFO_LOG(VIDEO, "\nCould not destroy display connection.");
  m_egl->ctx = EGL_NO_CONTEXT;
  m_egl->dpy = EGL_NO_DISPLAY;
}
