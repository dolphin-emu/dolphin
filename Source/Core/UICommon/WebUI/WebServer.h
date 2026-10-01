// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>

#include "picojson.h"

#include "Common/HookableEvent.h"

namespace WebUI
{

constexpr auto GBA_STREAM_COUNT = 4uz;

class AVStream;

class WebServer
{
public:
  explicit WebServer(std::string listen_ip_port);
  ~WebServer();

  std::weak_ptr<AVStream> GetGBAStream(std::size_t gba_index);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

void RefreshConfig();
void StartServer();
void StopServer();

// Callers should not lock() a shared_ptr for long periods.
std::weak_ptr<WebServer> GetServer();
std::weak_ptr<AVStream> GetGBAStream(std::size_t gba_index);

struct ServerEvents
{
  Common::HookableEvent<> server_started;
  Common::HookableEvent<> server_stopped;

  struct GBAEvents
  {
    // WebSocket connected to this GBA slot.
    Common::HookableEvent<> peer_connected;

    // FYI: The associated AVStream reference will have already been released.
    // i.e. If there are no remaining connections, GetGBAStream will produce nullptr.
    Common::HookableEvent<> peer_disconnected;

    // JSON message received on data channel.
    Common::HookableEvent<const picojson::object&> message_received;
  };
  std::array<GBAEvents, GBA_STREAM_COUNT> gba_events;
};

ServerEvents& GetServerEvents();

}  // namespace WebUI
