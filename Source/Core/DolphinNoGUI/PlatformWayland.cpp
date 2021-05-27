// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <unistd.h>

#include "DolphinNoGUI/Platform.h"

#include "Common/MsgHandler.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/State.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include <climits>
#include <cstdio>
#include <cstring>

#include <wayland-client-protocol.h>
#include "wayland-xdg-decoration-client-protocol.h"
#include "wayland-xdg-shell-client-protocol.h"

#include "UICommon/X11Utils.h"
#include "VideoCommon/RenderBase.h"

namespace
{
class PlatformWayland : public Platform
{
public:
  ~PlatformWayland() override;

  bool Init() override;
  void SetTitle(const std::string& string) override;
  void MainLoop() override;

  WindowSystemInfo GetWindowSystemInfo() const;

private:
  void ProcessEvents();

  static void GlobalRegistryHandler(void* data, wl_registry* registry, uint32_t id,
                                    const char* interface, uint32_t version);
  static void GlobalRegistryRemover(void* data, wl_registry* registry, uint32_t id);
  static void XDGWMBasePing(void* data, struct xdg_wm_base* xdg_wm_base, uint32_t serial);
  static void XDGSurfaceConfigure(void* data, struct xdg_surface* xdg_surface, uint32_t serial);
  static void TopLevelConfigure(void* data, struct xdg_toplevel* xdg_toplevel, int32_t width,
                                int32_t height, struct wl_array* states);
  static void TopLevelClose(void* data, struct xdg_toplevel* xdg_toplevel);

  wl_display* m_display = nullptr;
  wl_registry* m_registry = nullptr;
  wl_compositor* m_compositor = nullptr;
  xdg_wm_base* m_xdg_wm_base = nullptr;
  wl_surface* m_surface = nullptr;
  wl_region* m_region = nullptr;
  xdg_surface* m_xdg_surface = nullptr;
  xdg_toplevel* m_xdg_toplevel = nullptr;
  zxdg_decoration_manager_v1* m_decoration_manager = nullptr;
  zxdg_toplevel_decoration_v1* m_toplevel_decoration = nullptr;

  int m_surface_width = 0;
  int m_surface_height = 0;
};

PlatformWayland::~PlatformWayland()
{
  if (m_xdg_toplevel)
    xdg_toplevel_destroy(m_xdg_toplevel);
  if (m_xdg_surface)
    xdg_surface_destroy(m_xdg_surface);
  if (m_surface)
    wl_surface_destroy(m_surface);
  if (m_region)
    wl_region_destroy(m_region);
  if (m_xdg_wm_base)
    xdg_wm_base_destroy(m_xdg_wm_base);
  if (m_compositor)
    wl_compositor_destroy(m_compositor);
  if (m_registry)
    wl_registry_destroy(m_registry);
  if (m_display)
    wl_display_disconnect(m_display);
}

void PlatformWayland::GlobalRegistryHandler(void* data, wl_registry* registry, uint32_t id,
                                            const char* interface, uint32_t version)
{
  PlatformWayland* platform = static_cast<PlatformWayland*>(data);
  if (std::strcmp(interface, wl_compositor_interface.name) == 0)
  {
    platform->m_compositor = static_cast<wl_compositor*>(
        wl_registry_bind(platform->m_registry, id, &wl_compositor_interface, 1));
  }
  else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0)
  {
    platform->m_xdg_wm_base = static_cast<xdg_wm_base*>(
        wl_registry_bind(platform->m_registry, id, &xdg_wm_base_interface, 1));
  }
  else if (std::strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
  {
    platform->m_decoration_manager = static_cast<zxdg_decoration_manager_v1*>(
        wl_registry_bind(platform->m_registry, id, &zxdg_decoration_manager_v1_interface, 1));
  }
}

void PlatformWayland::GlobalRegistryRemover(void* data, wl_registry* registry, uint32_t id)
{
}

void PlatformWayland::XDGWMBasePing(void* data, struct xdg_wm_base* xdg_wm_base, uint32_t serial)
{
  xdg_wm_base_pong(xdg_wm_base, serial);
}

void PlatformWayland::XDGSurfaceConfigure(void* data, struct xdg_surface* xdg_surface,
                                          uint32_t serial)
{
  xdg_surface_ack_configure(xdg_surface, serial);
}

void PlatformWayland::TopLevelConfigure(void* data, struct xdg_toplevel* xdg_toplevel,
                                        int32_t width, int32_t height, struct wl_array* states)
{
  // If this is zero, it's asking us to set the size.
  if (width == 0 || height == 0)
    return;

  PlatformWayland* platform = static_cast<PlatformWayland*>(data);
  platform->m_surface_width = width;
  platform->m_surface_height = height;
  if (g_renderer)
    g_renderer->ResizeSurface(width, height);
  if (g_controller_interface.IsInit())
    g_controller_interface.OnWindowResized(width, height);
}

void PlatformWayland::TopLevelClose(void* data, struct xdg_toplevel* xdg_toplevel)
{
  PlatformWayland* platform = static_cast<PlatformWayland*>(data);
  platform->Stop();
}

bool PlatformWayland::Init()
{
  m_display = wl_display_connect(nullptr);
  if (!m_display)
  {
    PanicAlert("Failed to connect to Wayland display.");
    return false;
  }

  static const wl_registry_listener registry_listener = {GlobalRegistryHandler,
                                                         GlobalRegistryRemover};
  m_registry = wl_display_get_registry(m_display);
  wl_registry_add_listener(m_registry, &registry_listener, this);

  // Call back to registry listener to get compositor/shell.
  wl_display_dispatch(m_display);
  wl_display_roundtrip(m_display);

  // We need a shell/compositor, or at least one we understand.
  if (!m_compositor || !m_display || !m_xdg_wm_base)
  {
    std::fprintf(stderr, "Missing Wayland shell/compositor\n");
    return false;
  }

  // Create the compositor and shell surface.
  if (!(m_surface = wl_compositor_create_surface(m_compositor)) ||
      !(m_xdg_surface = xdg_wm_base_get_xdg_surface(m_xdg_wm_base, m_surface)) ||
      !(m_xdg_toplevel = xdg_surface_get_toplevel(m_xdg_surface)))
  {
    std::fprintf(stderr, "Failed to create compositor/shell surfaces\n");
    return false;
  }

  static const xdg_wm_base_listener xdg_wm_base_listener = {XDGWMBasePing};
  xdg_wm_base_add_listener(m_xdg_wm_base, &xdg_wm_base_listener, this);

  static const xdg_surface_listener shell_surface_listener = {XDGSurfaceConfigure};
  xdg_surface_add_listener(m_xdg_surface, &shell_surface_listener, this);

  static const xdg_toplevel_listener toplevel_listener = {TopLevelConfigure, TopLevelClose};
  xdg_toplevel_add_listener(m_xdg_toplevel, &toplevel_listener, this);

  // Create region in the surface to draw into.
  m_surface_width = Config::Get(Config::MAIN_RENDER_WINDOW_WIDTH);
  m_surface_height = Config::Get(Config::MAIN_RENDER_WINDOW_HEIGHT);
  m_region = wl_compositor_create_region(m_compositor);
  wl_region_add(m_region, 0, 0, m_surface_width, m_surface_height);
  wl_surface_set_opaque_region(m_surface, m_region);
  wl_surface_commit(m_surface);

  // This doesn't seem to have any effect on kwin...
  xdg_surface_set_window_geometry(m_xdg_surface, Config::Get(Config::MAIN_RENDER_WINDOW_XPOS),
                                  Config::Get(Config::MAIN_RENDER_WINDOW_YPOS),
                                  Config::Get(Config::MAIN_RENDER_WINDOW_WIDTH),
                                  Config::Get(Config::MAIN_RENDER_WINDOW_HEIGHT));

  if (m_decoration_manager)
  {
    m_toplevel_decoration =
        zxdg_decoration_manager_v1_get_toplevel_decoration(m_decoration_manager, m_xdg_toplevel);
    if (m_toplevel_decoration)
      zxdg_toplevel_decoration_v1_set_mode(m_toplevel_decoration,
                                           ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  }

  return true;
}

void PlatformWayland::SetTitle(const std::string& string)
{
  xdg_toplevel_set_title(m_xdg_toplevel, string.c_str());
}

void PlatformWayland::MainLoop()
{
  while (IsRunning())
  {
    UpdateRunningFlag();
    Core::HostDispatchJobs();
    ProcessEvents();

    // TODO: Is this sleep appropriate?
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

WindowSystemInfo PlatformWayland::GetWindowSystemInfo() const
{
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::Wayland;
  wsi.display_connection = static_cast<void*>(m_display);
  wsi.render_surface = reinterpret_cast<void*>(m_surface);
  wsi.render_surface_width = m_surface_width;
  wsi.render_surface_height = m_surface_height;
  return wsi;
}

void PlatformWayland::ProcessEvents()
{
  wl_display_dispatch_pending(m_display);
}
}  // namespace

std::unique_ptr<Platform> Platform::CreateWaylandPlatform()
{
  return std::make_unique<PlatformWayland>();
}
