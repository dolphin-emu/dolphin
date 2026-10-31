// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

struct wl_display;
struct wl_event_queue;
struct wl_interface;
struct wl_registry;

namespace Common::Wayland
{
// A private event queue on a wl_display owned by someone else (in Dolphin's case, Qt).
//
// Objects created through GetDisplay() and the registry are dispatched on our own queue, so
// Qt's event handling never sees their events. We never read from the socket outside of
// Roundtrip(): Qt's event loop does that, and reading in a second place would starve it of events.
// DispatchPending() therefore only delivers events which Qt has already read for us.
//
// All proxies created from this connection must be destroyed before the connection is.
class Connection
{
public:
  struct Global
  {
    u32 name;
    std::string interface;
    u32 version;
  };

  // Returns null if the registry cannot be enumerated.
  static std::unique_ptr<Connection> Create(wl_display* display);
  ~Connection();

  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  // A wrapper proxy for the display. Create new objects from this one.
  wl_display* GetDisplay() const { return m_display_wrapper; }

  const std::vector<Global>& GetGlobals() const { return m_globals; }
  const Global* FindGlobal(const char* interface_name) const;

  // Returns null if the global is not supported.
  void* Bind(const Global& global, const wl_interface& interface, u32 max_version);
  void* Bind(const wl_interface& interface, u32 max_version);

  // Waits until the compositor has processed everything we sent so far.
  bool Roundtrip();
  void Flush();
  // Dispatches events which are already queued. Never blocks.
  void DispatchPending();

private:
  Connection() = default;

  wl_display* m_display = nullptr;
  wl_display* m_display_wrapper = nullptr;
  wl_event_queue* m_queue = nullptr;
  wl_registry* m_registry = nullptr;
  std::vector<Global> m_globals;
};
}  // namespace Common::Wayland
