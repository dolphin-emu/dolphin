// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Wayland/Connection.h"

#include <algorithm>
#include <cstring>

#include <wayland-client.h>

#include "Common/Logging/Log.h"

namespace Common::Wayland
{
std::unique_ptr<Connection> Connection::Create(wl_display* display)
{
  if (!display)
    return nullptr;

  std::unique_ptr<Connection> connection(new Connection);
  connection->m_display = display;
  connection->m_queue = wl_display_create_queue(display);
  if (!connection->m_queue)
    return nullptr;

  connection->m_display_wrapper =
      static_cast<wl_display*>(wl_proxy_create_wrapper(static_cast<void*>(display)));
  if (!connection->m_display_wrapper)
    return nullptr;
  wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(connection->m_display_wrapper),
                     connection->m_queue);

  connection->m_registry = wl_display_get_registry(connection->m_display_wrapper);
  static constexpr wl_registry_listener listener = {
      .global =
          [](void* data, wl_registry*, u32 name, const char* interface, u32 version) {
            static_cast<Connection*>(data)->m_globals.push_back({name, interface, version});
          },
      .global_remove = [](void*, wl_registry*, u32) {},
  };
  wl_registry_add_listener(connection->m_registry, &listener, connection.get());

  if (!connection->Roundtrip())
  {
    WARN_LOG_FMT(COMMON, "Failed to enumerate Wayland globals");
    return nullptr;
  }

  return connection;
}

Connection::~Connection()
{
  if (m_registry)
    wl_registry_destroy(m_registry);
  if (m_display_wrapper)
    wl_proxy_wrapper_destroy(m_display_wrapper);
  if (m_queue)
    wl_event_queue_destroy(m_queue);
}

const Connection::Global* Connection::FindGlobal(const char* interface_name) const
{
  const auto it = std::ranges::find(m_globals, interface_name, &Global::interface);
  return it != m_globals.end() ? &*it : nullptr;
}

void* Connection::Bind(const Global& global, const wl_interface& interface, u32 max_version)
{
  return wl_registry_bind(m_registry, global.name, &interface,
                          std::min(global.version, max_version));
}

void* Connection::Bind(const wl_interface& interface, u32 max_version)
{
  const Global* global = FindGlobal(interface.name);
  return global ? Bind(*global, interface, max_version) : nullptr;
}

bool Connection::Roundtrip()
{
  return wl_display_roundtrip_queue(m_display, m_queue) >= 0;
}

void Connection::Flush()
{
  wl_display_flush(m_display);
}

void Connection::DispatchPending()
{
  wl_display_dispatch_queue_pending(m_display, m_queue);
}
}  // namespace Common::Wayland
