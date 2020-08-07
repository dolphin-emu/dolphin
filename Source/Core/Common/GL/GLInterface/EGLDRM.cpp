// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// EGL DRM GBM code based on RetroArch, RetroArch licenses follows

/*  RetroArch - A frontend for libretro.
 *  Copyright (c) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
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
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR 0x30FD
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR 0x31BD
#define EGL_NO_RESET_NOTIFICATION_KHR 0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_KHR 0x31BF
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR 0x00000002
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR 0x00000004
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR 0x00000002
#endif /* EGL_KHR_create_context */

using EGLAcceptConfigCB = bool (*)(void* display_data, EGLDisplay dpy, EGLConfig config);

typedef struct gfx_ctx_drm_data
{
  EGLContextData egl;
  int fd;
  int interval;
  unsigned fb_width;
  unsigned fb_height;

  bool core_hw_context_enable;
  bool waiting_for_flip;
  struct gbm_bo* bo;
  struct gbm_bo* next_bo;
  struct gbm_surface* gbm_surface;
  struct gbm_device* gbm_dev;
} gfx_ctx_drm_data_t;

struct drm_fb
{
  struct gbm_bo* bo;
  uint32_t fb_id;
};

/* TODO/FIXME - globals */
static drmEventContext g_drm_evctx;
static struct pollfd g_drm_fds;
static uint32_t g_connector_id = 0;
static int g_drm_fd = 0;
static uint32_t g_crtc_id = 0;
static drmModeCrtc* g_orig_crtc = NULL;
static drmModeConnector* g_drm_connector = NULL;
static drmModeModeInfo* g_drm_mode = NULL;

/* TODO/FIXME - static globals */
static drmModeRes* g_drm_resources = NULL;
static drmModeEncoder* g_drm_encoder = NULL;

static gfx_ctx_drm_data* g_drm = NULL;

bool drm_get_encoder(int fd);

/* Restore the original CRTC. */
void drm_restore_crtc(void);
bool drm_get_resources(int fd);
void drm_setup(int fd);
void drm_free(void);
bool drm_get_connector(int fd);
float drm_get_refresh_rate(void* data);

static void egl_destroy(EGLContextData* egl)
{
  if (egl->dpy)
  {
    eglMakeCurrent(egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (egl->ctx != EGL_NO_CONTEXT)
      eglDestroyContext(egl->dpy, egl->ctx);

    if (egl->surf != EGL_NO_SURFACE)
      eglDestroySurface(egl->dpy, egl->surf);
    eglTerminate(egl->dpy);
  }

  /* Be as careful as possible in deinit.
   * If we screw up, any TTY will not restore.
   */

  egl->ctx = EGL_NO_CONTEXT;
  egl->surf = EGL_NO_SURFACE;
  egl->dpy = EGL_NO_DISPLAY;
  egl->config = 0;
}

static void egl_swap_buffers(void* data)
{
  EGLContextData* egl = (EGLContextData*)data;
  if (egl && egl->dpy != EGL_NO_DISPLAY && egl->surf != EGL_NO_SURFACE)
  {
    eglSwapBuffers(egl->dpy, egl->surf);
  }
}

static void egl_set_swap_interval(EGLContextData* egl, int interval)
{
  /* Can be called before initialization.
   * Some contexts require that swap interval
   * is known at startup time.
   */
  egl->interval = interval;

  if (egl->dpy == EGL_NO_DISPLAY)
    return;
  if (!eglGetCurrentContext())
    return;

  INFO_LOG(VIDEO, "[EGL]: eglSwapInterval(%u)\n", interval);
  if (!eglSwapInterval(egl->dpy, interval))
  {
    INFO_LOG(VIDEO, "[EGL]: eglSwapInterval() failed 0x%x.\n", eglGetError());
  }
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

static bool check_egl_display_extension(void* data, const char* name)
{
  size_t nameLen;
  EGLContextData* egl = (EGLContextData*)data;
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

static EGLDisplay get_egl_display(EGLenum platform, void* native)
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
        EGLDisplay dpy = ptr_eglGetPlatformDisplay(platform, native, NULL);
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
        EGLDisplay dpy = ptr_eglGetPlatformDisplayEXT(platform, native, NULL);
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

static bool egl_get_native_visual_id(EGLContextData* egl, EGLint* value)
{
  if (!eglGetConfigAttrib(egl->dpy, egl->config, EGL_NATIVE_VISUAL_ID, value))
  {
    INFO_LOG(VIDEO, "[EGL]: egl_get_native_visual_id failed.\n");
    return false;
  }

  return true;
}

static bool egl_init_context_common(EGLContextData* egl, EGLint* count, const EGLint* attrib_ptr,
                                    egl_accept_config_cb_t cb, void* display_data)
{
  EGLint i;
  EGLint matched = 0;
  EGLConfig* configs = NULL;
  if (!egl)
    return false;

  if (!eglGetConfigs(egl->dpy, NULL, 0, count) || *count < 1)
  {
    INFO_LOG(VIDEO, "[EGL]: No configs to choose from.\n");
    return false;
  }

  configs = (EGLConfig*)malloc(*count * sizeof(*configs));
  if (!configs)
    return false;

  if (!eglChooseConfig(egl->dpy, attrib_ptr, configs, *count, &matched) || !matched)
  {
    INFO_LOG(VIDEO, "[EGL]: No EGL configs with appropriate attributes.\n");
    return false;
  }

  for (i = 0; i < *count; i++)
  {
    if (!cb || cb(display_data, egl->dpy, configs[i]))
    {
      egl->config = configs[i];
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

static bool egl_init_context(EGLContextData* egl, EGLenum platform, void* display_data,
                             EGLint* major, EGLint* minor, EGLint* count, const EGLint* attrib_ptr,
                             egl_accept_config_cb_t cb)
{
  EGLDisplay dpy = get_egl_display(platform, display_data);

  if (dpy == EGL_NO_DISPLAY)
  {
    INFO_LOG(VIDEO, "[EGL]: Couldn't get EGL display.\n");
    return false;
  }

  egl->dpy = dpy;

  if (!eglInitialize(egl->dpy, major, minor))
    return false;

  INFO_LOG(VIDEO, "[EGL]: EGL version: %d.%d\n", *major, *minor);

  return egl_init_context_common(egl, count, attrib_ptr, cb, display_data);
}

static bool egl_create_context(EGLContextData* egl, const EGLint* egl_attribs)
{
  EGLContext ctx = eglCreateContext(egl->dpy, egl->config, EGL_NO_CONTEXT, egl_attribs);

  if (ctx == EGL_NO_CONTEXT)
    return false;

  egl->ctx = ctx;

  return true;
}

static bool egl_create_surface(EGLContextData* egl, void* native_window)
{
  EGLint window_attribs[] = {
      EGL_RENDER_BUFFER,
      EGL_BACK_BUFFER,
      EGL_NONE,
  };

  egl->surf = eglCreateWindowSurface(egl->dpy, egl->config, (NativeWindowType)native_window,
                                     window_attribs);

  if (egl->surf == EGL_NO_SURFACE)
    return false;

  /* Connect the context to the surface. */
  if (!eglMakeCurrent(egl->dpy, egl->surf, egl->surf, egl->ctx))
    return false;

  INFO_LOG(VIDEO, "[EGL]: Current context: %p.\n", (void*)eglGetCurrentContext());

  return true;
}

static bool drm_wait_flip(int timeout)
{
  g_drm_fds.revents = 0;

  if (poll(&g_drm_fds, 1, timeout) < 0)
    return false;

  if (g_drm_fds.revents & (POLLHUP | POLLERR))
    return false;

  if (g_drm_fds.revents & POLLIN)
  {
    drmHandleEvent(g_drm_fd, &g_drm_evctx);
    return true;
  }

  return false;
}

/* Restore the original CRTC. */
void drm_restore_crtc(void)
{
  if (!g_orig_crtc)
    return;

  drmModeSetCrtc(g_drm_fd, g_orig_crtc->crtc_id, g_orig_crtc->buffer_id, g_orig_crtc->x,
                 g_orig_crtc->y, &g_connector_id, 1, &g_orig_crtc->mode);

  drmModeFreeCrtc(g_orig_crtc);
  g_orig_crtc = NULL;
}

bool drm_get_resources(int fd)
{
  g_drm_resources = drmModeGetResources(fd);
  if (!g_drm_resources)
  {
    INFO_LOG(VIDEO, "[DRM]: Couldn't get device resources.\n");
    return false;
  }

  return true;
}

bool drm_get_connector(int fd)
{
  unsigned i;
  unsigned monitor_index_count = 0;
  unsigned monitor = 1;

  /* Enumerate all connectors. */

  INFO_LOG(VIDEO, "[DRM]: Found %d connectors.\n", g_drm_resources->count_connectors);

  for (i = 0; (int)i < g_drm_resources->count_connectors; i++)
  {
    drmModeConnectorPtr conn = drmModeGetConnector(fd, g_drm_resources->connectors[i]);

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

  for (i = 0; (int)i < g_drm_resources->count_connectors; i++)
  {
    g_drm_connector = drmModeGetConnector(fd, g_drm_resources->connectors[i]);

    if (!g_drm_connector)
      continue;
    if (g_drm_connector->connection == DRM_MODE_CONNECTED && g_drm_connector->count_modes > 0)
    {
      monitor_index_count++;
      if (monitor_index_count == monitor)
        break;
    }

    drmModeFreeConnector(g_drm_connector);
    g_drm_connector = NULL;
  }

  if (!g_drm_connector)
  {
    INFO_LOG(VIDEO, "[DRM]: Couldn't get device connector.\n");
    return false;
  }
  return true;
}

bool drm_get_encoder(int fd)
{
  unsigned i;

  for (i = 0; (int)i < g_drm_resources->count_encoders; i++)
  {
    g_drm_encoder = drmModeGetEncoder(fd, g_drm_resources->encoders[i]);

    if (!g_drm_encoder)
      continue;

    if (g_drm_encoder->encoder_id == g_drm_connector->encoder_id)
      break;

    drmModeFreeEncoder(g_drm_encoder);
    g_drm_encoder = NULL;
  }

  if (!g_drm_encoder)
  {
    INFO_LOG(VIDEO, "[DRM]: Couldn't find DRM encoder.\n");
    return false;
  }

  for (i = 0; (int)i < g_drm_connector->count_modes; i++)
  {
    INFO_LOG(VIDEO, "[DRM]: Mode %d: (%s) %d x %d, %u Hz\n", i, g_drm_connector->modes[i].name,
             g_drm_connector->modes[i].hdisplay, g_drm_connector->modes[i].vdisplay,
             g_drm_connector->modes[i].vrefresh);
  }

  return true;
}

void drm_setup(int fd)
{
  g_crtc_id = g_drm_encoder->crtc_id;
  g_connector_id = g_drm_connector->connector_id;
  g_orig_crtc = drmModeGetCrtc(fd, g_crtc_id);
  if (!g_orig_crtc)
    INFO_LOG(VIDEO, "[DRM]: Cannot find original CRTC.\n");
}

float drm_get_refresh_rate(void* data)
{
  float refresh_rate = 0.0f;

  if (g_drm_mode)
  {
    refresh_rate = g_drm_mode->clock * 1000.0f / g_drm_mode->htotal / g_drm_mode->vtotal;
  }

  return refresh_rate;
}

void drm_free(void)
{
  if (g_drm_encoder)
    drmModeFreeEncoder(g_drm_encoder);
  if (g_drm_connector)
    drmModeFreeConnector(g_drm_connector);
  if (g_drm_resources)
    drmModeFreeResources(g_drm_resources);

  memset(&g_drm_fds, 0, sizeof(struct pollfd));
  memset(&g_drm_evctx, 0, sizeof(drmEventContext));

  g_drm_encoder = NULL;
  g_drm_connector = NULL;
  g_drm_resources = NULL;
}

static void drm_fb_destroy_callback(struct gbm_bo* bo, void* data)
{
  struct drm_fb* fb = (struct drm_fb*)data;

  if (fb && fb->fb_id)
    drmModeRmFB(g_drm_fd, fb->fb_id);

  free(fb);
}

static struct drm_fb* drm_fb_get_from_bo(struct gbm_bo* bo)
{
  int ret;
  unsigned width, height, stride, handle;
  struct drm_fb* fb = (struct drm_fb*)calloc(1, sizeof(*fb));

  fb->bo = bo;

  width = gbm_bo_get_width(bo);
  height = gbm_bo_get_height(bo);
  stride = gbm_bo_get_stride(bo);
  handle = gbm_bo_get_handle(bo).u32;

  INFO_LOG(VIDEO, "[KMS]: New FB: %ux%u (stride: %u).\n", width, height, stride);

  ret = drmModeAddFB(g_drm_fd, width, height, 24, 32, stride, handle, &fb->fb_id);
  if (ret < 0)
    goto error;

  gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);
  return fb;

error:
  INFO_LOG(VIDEO, "[KMS]: Failed to create FB: %s\n", strerror(errno));
  free(fb);
  return NULL;
}

static void gfx_ctx_drm_swap_interval(void* data, int interval)
{
  gfx_ctx_drm_data_t* drm = (gfx_ctx_drm_data_t*)data;
  drm->interval = interval;

  if (interval > 1)
    INFO_LOG(VIDEO,
             "[KMS]: Swap intervals > 1 currently not supported. Will use swap interval of 1.\n");
}

static void drm_flip_handler(int fd, unsigned frame, unsigned sec, unsigned usec, void* data)
{
#if 0
   static unsigned first_page_flip;
   static unsigned last_page_flip;

   if (!first_page_flip)
      first_page_flip = frame;

   if (last_page_flip)
   {
      unsigned missed = frame - last_page_flip - 1;
      if (missed)
         INFO_LOG(VIDEO, "[KMS]: Missed %u VBlank(s) (Frame: %u, DRM frame: %u).\n",
               missed, frame - first_page_flip, frame);
   }

   last_page_flip = frame;
#endif

  *(bool*)data = false;
}

static bool gfx_ctx_drm_wait_flip(gfx_ctx_drm_data_t* drm, bool block)
{
  int timeout = 0;

  if (!drm->waiting_for_flip)
    return false;

  if (block)
    timeout = -1;

  while (drm->waiting_for_flip)
  {
    if (!drm_wait_flip(timeout))
      break;
  }

  if (drm->waiting_for_flip)
  {
    INFO_LOG(VIDEO, "\nwait flip 2");
    return true;
  }

  /* Page flip has taken place. */

  /* This buffer is not on-screen anymore. Release it to GBM. */
  gbm_surface_release_buffer(drm->gbm_surface, drm->bo);
  /* This buffer is being shown now. */
  drm->bo = drm->next_bo;
  return false;
}

static bool gfx_ctx_drm_queue_flip(gfx_ctx_drm_data_t* drm)
{
  struct drm_fb* fb = NULL;

  drm->next_bo = gbm_surface_lock_front_buffer(drm->gbm_surface);
  fb = (struct drm_fb*)gbm_bo_get_user_data(drm->next_bo);

  if (!fb)
    fb = (struct drm_fb*)drm_fb_get_from_bo(drm->next_bo);

  if (drmModePageFlip(g_drm_fd, g_crtc_id, fb->fb_id, DRM_MODE_PAGE_FLIP_EVENT,
                      &drm->waiting_for_flip) == 0)
    return true;

  /* Failed to queue page flip. */
  INFO_LOG(VIDEO, "\nFailed to queue page flip\n");
  return false;
}

static void gfx_ctx_drm_swap_buffers(void* data)
{
  gfx_ctx_drm_data_t* drm = (gfx_ctx_drm_data_t*)data;
  unsigned max_swapchain_images = 2;  // settings->uints.video_max_swapchain_images;

  egl_swap_buffers(&drm->egl);

  /* I guess we have to wait for flip to have taken
   * place before another flip can be queued up.
   *
   * If true, we are still waiting for a flip
   * (nonblocking mode, so just drop the frame). */
  if (gfx_ctx_drm_wait_flip(drm, drm->interval))
  {
    INFO_LOG(VIDEO, "\nwait flip");
    return;
  }

  drm->waiting_for_flip = gfx_ctx_drm_queue_flip(drm);

  /* Triple-buffered page flips */
  if (max_swapchain_images >= 3 && gbm_surface_has_free_buffers(drm->gbm_surface))
    return;

  gfx_ctx_drm_wait_flip(drm, true);
}

static void gfx_ctx_drm_get_video_size(void* data, unsigned* width, unsigned* height)
{
  gfx_ctx_drm_data_t* drm = (gfx_ctx_drm_data_t*)data;

  if (!drm)
  {
    INFO_LOG(VIDEO, "\nCannot  get drm video size\n");
    return;
  }

  *width = drm->fb_width;
  *height = drm->fb_height;
}

static void free_drm_resources(gfx_ctx_drm_data_t* drm)
{
  if (!drm)
    return;

  /* Restore original CRTC. */
  drm_restore_crtc();

  if (drm->gbm_surface)
    gbm_surface_destroy(drm->gbm_surface);

  if (drm->gbm_dev)
    gbm_device_destroy(drm->gbm_dev);

  drm_free();

  if (drm->fd >= 0)
  {
    if (g_drm_fd >= 0)
    {
      drmDropMaster(g_drm_fd);
      close(drm->fd);
    }
  }

  drm->gbm_surface = NULL;
  drm->gbm_dev = NULL;
  g_drm_fd = -1;
}

static void gfx_ctx_drm_destroy_resources(gfx_ctx_drm_data_t* drm)
{
  if (!drm)
    return;

  /* Make sure we acknowledge all page-flips. */
  gfx_ctx_drm_wait_flip(drm, true);

  egl_destroy(&drm->egl);

  free_drm_resources(drm);

  g_drm_mode = NULL;
  g_crtc_id = 0;
  g_connector_id = 0;

  drm->fb_width = 0;
  drm->fb_height = 0;

  drm->bo = NULL;
  drm->next_bo = NULL;
}

static void* gfx_ctx_drm_init()
{
  int fd, i;
  unsigned monitor_index;
  unsigned gpu_index = 0;
  const char* gpu = NULL;
  gfx_ctx_drm_data_t* drm = (gfx_ctx_drm_data_t*)calloc(1, sizeof(gfx_ctx_drm_data_t));

  if (!drm)
    return NULL;
  drm->fd = -1;

  free_drm_resources(drm);

  drm->fd = open("/dev/dri/card0", O_RDWR);
  if (drm->fd < 0)
  {
    INFO_LOG(VIDEO, "[KMS]: Couldn't open DRM device.\n");
    return nullptr;
  }

  fd = drm->fd;

  if (!drm_get_resources(fd))
  {
    INFO_LOG(VIDEO, "[KMS]: drm_get_resources failed\n");
    return nullptr;
  }

  if (!drm_get_connector(fd))
  {
    INFO_LOG(VIDEO, "[KMS]: drm_get_connector failed\n");
    return nullptr;
  }

  if (!drm_get_encoder(fd))
  {
    INFO_LOG(VIDEO, "[KMS]: drm_get_encoder failed\n");
    return nullptr;
  }

  drm_setup(fd);

  /* Choose the optimal video mode for get_video_size():
    - the current video mode from the CRTC
    - otherwise pick first connector mode */
  if (g_orig_crtc->mode_valid)
  {
    drm->fb_width = g_orig_crtc->mode.hdisplay;
    drm->fb_height = g_orig_crtc->mode.vdisplay;
  }
  else
  {
    drm->fb_width = g_drm_connector->modes[0].hdisplay;
    drm->fb_height = g_drm_connector->modes[0].vdisplay;
  }

  drmSetMaster(g_drm_fd);

  drm->gbm_dev = gbm_create_device(fd);

  if (!drm->gbm_dev)
  {
    INFO_LOG(VIDEO, "[KMS]: Couldn't create GBM device.\n");
    return nullptr;
  }

  /* Setup the flip handler. */
  g_drm_fds.fd = fd;
  g_drm_fds.events = POLLIN;
  g_drm_evctx.version = DRM_EVENT_CONTEXT_VERSION;
  g_drm_evctx.page_flip_handler = drm_flip_handler;

  g_drm_fd = fd;

  return drm;

error:
  gfx_ctx_drm_destroy_resources(drm);

  if (drm)
    free(drm);

  return NULL;
}

static EGLint* gfx_ctx_drm_egl_fill_attribs(gfx_ctx_drm_data_t* drm, EGLint* attr)
{
  *attr++ = EGL_CONTEXT_CLIENT_VERSION;
  *attr++ = drm->egl.major ? (EGLint)drm->egl.major : 2;
#ifdef EGL_KHR_create_context
  if (drm->egl.minor > 0)
  {
    *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
    *attr++ = drm->egl.minor;
  }
#endif

  *attr = EGL_NONE;
  return attr;
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

static bool gfx_ctx_drm_egl_set_video_mode(gfx_ctx_drm_data_t* drm)
{
  const EGLint* attrib_ptr = NULL;
  EGLint major;
  EGLint minor;
  EGLint n;
  EGLint egl_attribs[16];
  EGLint* egl_attribs_ptr = NULL;
  EGLint* attr = NULL;

  attrib_ptr = egl_attribs_gles3;

  if (!egl_init_context(&drm->egl, EGL_PLATFORM_GBM_KHR, (EGLNativeDisplayType)drm->gbm_dev, &major,
                        &minor, &n, attrib_ptr, gbm_choose_xrgb8888_cb))
  {
    INFO_LOG(VIDEO, "\n[EGL] Cannot init context error 0x%x", eglGetError());
    goto error;
  }
  attr = gfx_ctx_drm_egl_fill_attribs(drm, egl_attribs);
  egl_attribs_ptr = &egl_attribs[0];

  if (!egl_create_context(&drm->egl, (attr != egl_attribs_ptr) ? egl_attribs_ptr : NULL))
  {
    INFO_LOG(VIDEO, "\n[EGL] Cannot create context error 0x%x", eglGetError());
    goto error;
  }

  if (!egl_create_surface(&drm->egl, (EGLNativeWindowType)drm->gbm_surface))
  {
    INFO_LOG(VIDEO, "\n[EGL] Cannot create context error 0x%x", eglGetError());
    return false;
  }

  egl_swap_buffers(&drm->egl);

  return true;

error:
  return false;
}

static bool gfx_ctx_drm_set_video_mode(void* data, unsigned width, unsigned height, bool fullscreen)
{
  int i, ret = 0;
  struct drm_fb* fb = NULL;
  gfx_ctx_drm_data_t* drm = (gfx_ctx_drm_data_t*)data;
  float video_refresh_rate = (float)VideoInterface::GetTargetRefreshRate();

  if (!drm)
    return false;

  /* Find desired video mode, and use that.
   * If not fullscreen, we get desired windowed size,
   * which is not appropriate. */
  if ((width == 0 && height == 0) || !fullscreen)
    g_drm_mode = &g_drm_connector->modes[0];
  else
  {
    /* Try to match refresh_rate as closely as possible.
     *
     * Lower resolutions tend to have multiple supported
     * refresh rates as well.
     */
    float minimum_fps_diff = 0.0f;

    /* Find best match. */
    for (i = 0; i < g_drm_connector->count_modes; i++)
    {
      float diff;
      if (width != g_drm_connector->modes[i].hdisplay ||
          height != g_drm_connector->modes[i].vdisplay)
        continue;

      diff = fabsf(g_drm_connector->modes[i].vrefresh - video_refresh_rate);

      if (!g_drm_mode || diff < minimum_fps_diff)
      {
        g_drm_mode = &g_drm_connector->modes[i];
        minimum_fps_diff = diff;
      }
    }
  }

  if (!g_drm_mode)
  {
    INFO_LOG(VIDEO, "[KMS/EGL]: Did not find suitable video mode for %u x %u.\n", width, height);
    goto error;
  }

  drm->fb_width = g_drm_mode->hdisplay;
  drm->fb_height = g_drm_mode->vdisplay;

  /* Create GBM surface. */
  drm->gbm_surface =
      gbm_surface_create(drm->gbm_dev, drm->fb_width, drm->fb_height, GBM_FORMAT_XRGB8888,
                         GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

  if (!drm->gbm_surface)
  {
    INFO_LOG(VIDEO, "[KMS/EGL]: Couldn't create GBM surface.\n");
    goto error;
  }

  if (!gfx_ctx_drm_egl_set_video_mode(drm))
  {
    INFO_LOG(VIDEO, "[KMS/EGL]: Couldn't set EGL video mode.\n");
    goto error;
  }

  drm->bo = gbm_surface_lock_front_buffer(drm->gbm_surface);

  fb = (struct drm_fb*)gbm_bo_get_user_data(drm->bo);

  if (!fb)
    fb = drm_fb_get_from_bo(drm->bo);

  ret = drmModeSetCrtc(g_drm_fd, g_crtc_id, fb->fb_id, 0, 0, &g_connector_id, 1, g_drm_mode);
  if (ret < 0)
  {
    INFO_LOG(VIDEO, "[KMS/EGL]: drmModeSetCrtc failed\n");
    goto error;
  }
  return true;

error:
  gfx_ctx_drm_destroy_resources(drm);

  if (drm)
    free(drm);

  return false;
}

static void gfx_ctx_drm_destroy(void* data)
{
  gfx_ctx_drm_data_t* drm = (gfx_ctx_drm_data_t*)data;

  if (!drm)
    return;

  gfx_ctx_drm_destroy_resources(drm);
  free(drm);
}

GLContextEGLDRM::~GLContextEGLDRM()
{
  DestroyWindowSurface();
  DestroyContext();
  gfx_ctx_drm_destroy(g_drm);
}

bool GLContextEGLDRM::IsHeadless() const
{
  return false;
}

void GLContextEGLDRM::Swap()
{
  gfx_ctx_drm_swap_buffers(g_drm);
}
void GLContextEGLDRM::SwapInterval(int interval)
{
  gfx_ctx_drm_swap_interval(g_drm, interval);
  egl_set_swap_interval(m_egl, interval);
}

void* GLContextEGLDRM::GetFuncAddress(const std::string& name)
{
  return (void*)eglGetProcAddress(name.c_str());
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool GLContextEGLDRM::Initialize(const WindowSystemInfo& wsi, bool stereo, bool core)
{
  g_drm = (gfx_ctx_drm_data_t*)gfx_ctx_drm_init();

  eglBindAPI(EGL_OPENGL_ES_API);

  // Use current video mode, do not switch
  gfx_ctx_drm_set_video_mode(g_drm, 0, 0, false);
  m_backbuffer_width = g_drm->fb_width;
  m_backbuffer_height = g_drm->fb_height;

  m_egl = &g_drm->egl;
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
  EGLint* egl_attribs_ptr = NULL;
  const EGLint* attrib_ptr = egl_attribs_gles3;
  EGLint* attr = gfx_ctx_drm_egl_fill_attribs(g_drm, egl_attribs);
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
    INFO_LOG(VIDEO, "\nCreated surfaceless context\n");
    return true;
  }

  if (!IsHeadless())
  {
    if (!egl_create_surface(m_egl, (EGLNativeWindowType)g_drm->gbm_surface))
    {
      INFO_LOG(VIDEO, "\negl_create_surface failed, trying pbuffer failed 0x%x\n", eglGetError());
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
  return eglMakeCurrent(m_egl->dpy, g_drm->egl.surf, g_drm->egl.surf, m_egl->ctx);
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
