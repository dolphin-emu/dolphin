// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinNoGUI/Platform.h"

#include <cstdio>
#include <cstring>
#include <poll.h>
#include <string>

#include <wayland-client.h>
#include <wayland-webos-shell-client-protocol.h>

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/System.h"

namespace
{
class PlatformWayland final : public Platform
{
public:
  ~PlatformWayland() override;

  bool Init() override;
  void SetTitle(const std::string& title) override;
  void MainLoop() override;

  WindowSystemInfo GetWindowSystemInfo() const override;

private:
  static void RegistryGlobal(void* data, wl_registry* registry, uint32_t id, const char* interface,
                             uint32_t version);
  static void RegistryGlobalRemove(void* data, wl_registry* registry, uint32_t id);
  static void ShellSurfaceClose(void* data, wl_webos_shell_surface* surface);

  void Destroy();
  void DispatchPending();

  wl_display* m_display = nullptr;
  wl_registry* m_registry = nullptr;
  wl_compositor* m_compositor = nullptr;
  wl_shell* m_shell = nullptr;
  wl_webos_shell* m_webos_shell = nullptr;
  wl_surface* m_surface = nullptr;
  wl_shell_surface* m_shell_surface = nullptr;
  wl_webos_shell_surface* m_webos_shell_surface = nullptr;

  int m_width = 1920;
  int m_height = 1080;
};

void PlatformWayland::RegistryGlobal(void* data, wl_registry* registry, uint32_t id,
                                     const char* interface, uint32_t version)
{
  auto* self = static_cast<PlatformWayland*>(data);
  if (std::strcmp(interface, "wl_compositor") == 0)
  {
    self->m_compositor = static_cast<wl_compositor*>(
        wl_registry_bind(registry, id, &wl_compositor_interface, version < 3 ? version : 3));
  }
  else if (std::strcmp(interface, "wl_shell") == 0)
  {
    self->m_shell =
        static_cast<wl_shell*>(wl_registry_bind(registry, id, &wl_shell_interface, 1));
  }
  else if (std::strcmp(interface, "wl_webos_shell") == 0)
  {
    self->m_webos_shell = static_cast<wl_webos_shell*>(
        wl_registry_bind(registry, id, &wl_webos_shell_interface, 1));
  }
}

void PlatformWayland::RegistryGlobalRemove(void* /*data*/, wl_registry* /*registry*/,
                                           uint32_t /*id*/)
{
}

void PlatformWayland::ShellSurfaceClose(void* data, wl_webos_shell_surface* /*surface*/)
{
  static_cast<PlatformWayland*>(data)->RequestShutdown();
}

PlatformWayland::~PlatformWayland()
{
  Destroy();
}

void PlatformWayland::Destroy()
{
  if (m_webos_shell_surface)
  {
    wl_webos_shell_surface_destroy(m_webos_shell_surface);
    m_webos_shell_surface = nullptr;
  }
  if (m_shell_surface)
  {
    wl_shell_surface_destroy(m_shell_surface);
    m_shell_surface = nullptr;
  }
  if (m_surface)
  {
    wl_surface_destroy(m_surface);
    m_surface = nullptr;
  }
  if (m_webos_shell)
  {
    wl_webos_shell_destroy(m_webos_shell);
    m_webos_shell = nullptr;
  }
  if (m_shell)
  {
    wl_shell_destroy(m_shell);
    m_shell = nullptr;
  }
  if (m_compositor)
  {
    wl_compositor_destroy(m_compositor);
    m_compositor = nullptr;
  }
  if (m_registry)
  {
    wl_registry_destroy(m_registry);
    m_registry = nullptr;
  }
  if (m_display)
  {
    wl_display_disconnect(m_display);
    m_display = nullptr;
  }
}

bool PlatformWayland::Init()
{
  m_width = Config::Get(Config::MAIN_RENDER_WINDOW_WIDTH);
  m_height = Config::Get(Config::MAIN_RENDER_WINDOW_HEIGHT);
  if (m_width <= 0)
    m_width = 1920;
  if (m_height <= 0)
    m_height = 1080;

  m_display = wl_display_connect(nullptr);
  if (!m_display)
  {
    std::fprintf(stderr, "PlatformWayland: wl_display_connect failed\n");
    return false;
  }

  static const wl_registry_listener registry_listener = {RegistryGlobal, RegistryGlobalRemove};
  m_registry = wl_display_get_registry(m_display);
  wl_registry_add_listener(m_registry, &registry_listener, this);
  wl_display_roundtrip(m_display);

  if (!m_compositor || !m_shell || !m_webos_shell)
  {
    std::fprintf(stderr,
                 "PlatformWayland: missing compositor/shell/webos_shell (got %p/%p/%p)\n",
                 static_cast<void*>(m_compositor), static_cast<void*>(m_shell),
                 static_cast<void*>(m_webos_shell));
    return false;
  }

  m_surface = wl_compositor_create_surface(m_compositor);
  if (!m_surface)
  {
    std::fprintf(stderr, "PlatformWayland: wl_compositor_create_surface failed\n");
    return false;
  }

  m_shell_surface = wl_shell_get_shell_surface(m_shell, m_surface);
  if (!m_shell_surface)
  {
    std::fprintf(stderr, "PlatformWayland: wl_shell_get_shell_surface failed\n");
    return false;
  }
  wl_shell_surface_set_toplevel(m_shell_surface);

  m_webos_shell_surface = wl_webos_shell_get_shell_surface(m_webos_shell, m_surface);
  if (!m_webos_shell_surface)
  {
    std::fprintf(stderr, "PlatformWayland: wl_webos_shell_get_shell_surface failed\n");
    return false;
  }

  static const wl_webos_shell_surface_listener webos_listener = {
      .state_changed = nullptr,
      .position_changed = nullptr,
      .close = ShellSurfaceClose,
      .exposed = nullptr,
      .state_about_to_change = nullptr,
      .addon_status_changed = nullptr,
  };
  wl_webos_shell_surface_add_listener(m_webos_shell_surface, &webos_listener, this);

  const char* app_id = std::getenv("APPID");
  if (!app_id || !*app_id || std::strcmp(app_id, "com.palm.devmode.openssh") == 0)
    app_id = "org.dolphinemu.webos";

  const char* display_id = std::getenv("DISPLAY_ID");
  if (!display_id || !*display_id)
    display_id = "0";

  wl_webos_shell_surface_set_property(m_webos_shell_surface, "appId", app_id);
  wl_webos_shell_surface_set_property(m_webos_shell_surface, "title", "Dolphin");
  wl_webos_shell_surface_set_property(m_webos_shell_surface, "displayAffinity", display_id);
  wl_webos_shell_surface_set_property(m_webos_shell_surface, "_WEBOS_ACCESS_POLICY_KEYS_BACK",
                                      "true");
  wl_webos_shell_surface_set_property(m_webos_shell_surface, "_WEBOS_ACCESS_POLICY_KEYS_EXIT",
                                      "true");

  if (Config::Get(Config::MAIN_FULLSCREEN))
  {
    wl_webos_shell_surface_set_state(m_webos_shell_surface,
                                     WL_WEBOS_SHELL_SURFACE_STATE_FULLSCREEN);
    m_window_fullscreen = true;
  }

  wl_surface_commit(m_surface);
  wl_display_roundtrip(m_display);
  return true;
}

void PlatformWayland::SetTitle(const std::string& title)
{
  if (m_webos_shell_surface)
    wl_webos_shell_surface_set_property(m_webos_shell_surface, "title", title.c_str());
}

void PlatformWayland::DispatchPending()
{
  if (!m_display)
    return;
  wl_display_dispatch_pending(m_display);
  wl_display_flush(m_display);
}

void PlatformWayland::MainLoop()
{
  while (IsRunning())
  {
    UpdateRunningFlag();
    Core::HostDispatchJobs(Core::System::GetInstance());
    DispatchPending();
    // Block briefly for Wayland events without spinning the CPU.
    if (wl_display_prepare_read(m_display) == 0)
    {
      wl_display_flush(m_display);
      pollfd pfd{};
      pfd.fd = wl_display_get_fd(m_display);
      pfd.events = POLLIN;
      const int ret = poll(&pfd, 1, 1);
      if (ret > 0)
        wl_display_read_events(m_display);
      else
        wl_display_cancel_read(m_display);
    }
    else
    {
      wl_display_dispatch_pending(m_display);
    }
  }
}

WindowSystemInfo PlatformWayland::GetWindowSystemInfo() const
{
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::Wayland;
  wsi.display_connection = m_display;
  wsi.render_window = m_surface;
  wsi.render_surface = m_surface;
  wsi.render_surface_width = m_width;
  wsi.render_surface_height = m_height;
  return wsi;
}
}  // namespace

std::unique_ptr<Platform> Platform::CreateWaylandPlatform()
{
  return std::make_unique<PlatformWayland>();
}
