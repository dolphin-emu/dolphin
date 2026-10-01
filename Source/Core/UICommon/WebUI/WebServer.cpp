// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/WebUI/WebServer.h"

#include <latch>
#include <set>
#include <shared_mutex>
#include <thread>

#include <httplib.h>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/Thread.h"

#include "Core/Config/MainSettings.h"

#include "UICommon/WebUI/AVStream.h"

namespace
{
// Holds a weak pointer. Allows atomically getting or creating the instance.
template <typename T>
class WeakInstance
{
public:
  std::weak_ptr<T> Get()
  {
    std::shared_lock lk{m_mutex};
    return m_ptr;
  }

  std::shared_ptr<T> GetOrCreate()
  {
    std::lock_guard lk{m_mutex};
    auto ptr = m_ptr.lock();
    if (!ptr)
      m_ptr = ptr = std::make_shared<T>();
    return ptr;
  }

private:
  std::shared_mutex m_mutex;
  std::weak_ptr<T> m_ptr;
};

std::shared_mutex g_server_mutex;
std::shared_ptr<WebUI::WebServer> g_web_server;

std::weak_ptr<WebUI::WebServer> g_stopping_web_server;

void StartServer()
{
  std::lock_guard lk{g_server_mutex};

  if (g_web_server != nullptr)
    return;  // Already running.

  // The stopping server `shared_ptr` is still held elsewhere via GetServer().
  while (auto stopping_server = g_stopping_web_server.lock()) [[unlikely]]
  {
    INFO_LOG_FMT(COMMON, "StartServer: Waiting for previous server to stop.");
    std::latch server_stopped{1};
    const auto hook =
        WebUI::GetServerEvents().server_stopped.Register([&] { server_stopped.count_down(1); });

    // Release our reference and wait for the destructor signal.
    stopping_server.reset();
    server_stopped.wait();
  }

  g_web_server =
      std::make_shared<WebUI::WebServer>(Config::Get(Config::MAIN_WEB_INTERFACE_SERVER_PORT));
}

void StopServer()
{
  decltype(g_web_server) web_server;
  {
    std::lock_guard lk{g_server_mutex};

    if (g_web_server == nullptr)
      return;  // Already stopped (or stopping).

    web_server.swap(g_web_server);
  }

  // Maintain a `weak_ptr`. Another thread may have a transient `shared_ptr` via GetServer().
  g_stopping_web_server = web_server;
}

class GBAWebSocket
{
public:
  GBAWebSocket(const std::size_t gba_index, httplib::ws::WebSocket& web_socket)
      : m_gba_index{gba_index}, m_web_socket{web_socket}
  {
  }

  void operator()(const httplib::Request& request, WeakInstance<WebUI::AVStream>& av_stream)
  {
    NOTICE_LOG_FMT(COMMON, "WebUI: GBA{} WebSocket connected: {}:{}", m_gba_index + 1,
                   request.remote_addr, request.remote_port);

    auto& gba_events = WebUI::GetServerEvents().gba_events[m_gba_index];

    {
      // FYI: The AVStreamPeer controls the life of the AVStream.
      WebUI::AVStreamPeer peer{av_stream.GetOrCreate()};

      gba_events.peer_connected.Trigger();

      peer.SetSendMessageCallback([&](const std::string& msg) { m_web_socket.send(msg); });

      // Connect received text messages to the global event for this GBA slot.
      peer.SetControlMessageCallback(
          [&](const picojson::object& message) { gba_events.message_received.Trigger(message); });

      while (m_web_socket.is_open() && ReadMessage(peer))
      {
      }
    }

    gba_events.peer_disconnected.Trigger();

    NOTICE_LOG_FMT(COMMON, "WebUI: GBA{} WebSocket disconnected", m_gba_index + 1);
  }

private:
  bool ReadMessage(WebUI::AVStreamPeer& peer)
  {
    std::string message;
    switch (m_web_socket.read(message))
    {
    case httplib::ws::ReadResult::Text:
      DEBUG_LOG_FMT(COMMON, "WebUI: GBA{} received: {}", m_gba_index + 1, message);
      return peer.HandleMessage(message);

    case httplib::ws::ReadResult::Timeout:
      return true;

    default:
      break;
    }

    return false;
  }

  const std::size_t m_gba_index;
  httplib::ws::WebSocket& m_web_socket;
};

}  // namespace

namespace WebUI
{

std::weak_ptr<WebServer> GetServer()
{
  std::shared_lock lk{g_server_mutex};
  return g_web_server;
}

std::weak_ptr<AVStream> GetGBAStream(std::size_t gba_index)
{
  if (const auto server = GetServer().lock())
    return server->GetGBAStream(gba_index);
  return {};
}

struct WebServer::Impl
{
  explicit Impl(const std::string& listen_ip_port)
  {
    std::string bind_ip = "0.0.0.0";
    u16 bind_port = 0;

    const auto ip_port_parts = SplitString(listen_ip_port, ':');

    bool parse_okay = false;
    if (ip_port_parts.size() == 1)
    {
      // Just a port.
      parse_okay = TryParse(ip_port_parts[0], &bind_port);
    }
    else if (ip_port_parts.size() == 2)
    {
      // IP and port.
      bind_ip = ip_port_parts[0];
      parse_okay = TryParse(ip_port_parts[1], &bind_port);
    }

    if (!parse_okay)
    {
      ERROR_LOG_FMT(COMMON, "WebServer: Bad port config: {}", listen_ip_port);
      return;
    }

    m_thread = std::thread{&Impl::ThreadFunc, this, std::move(bind_ip), bind_port};
  }

  ~Impl()
  {
    m_server.wait_until_ready();

    INFO_LOG_FMT(COMMON, "WebUI: Stopping server");
    m_server.stop();

    {
      std::lock_guard lk{m_web_socket_mutex};
      for (auto* ws : m_web_sockets)
        ws->close(httplib::ws::CloseStatus::GoingAway);
    }

    if (m_thread.joinable())
      m_thread.join();

    GetServerEvents().server_stopped.Trigger();
  }

  void ThreadFunc(std::string bind_ip, u16 bind_port)
  {
    Common::SetCurrentThreadName("WebUI Server");

    Common::ScopeGuard decommission{[this] { m_server.decommission(); }};

    // Serve files from the Sys/ directory.
    m_server.set_mount_point("/", File::GetSysDirectory() + "/WebUI");

    // Serve AV streams at /gba{1..4}
    for (std::size_t gba_index = 0; gba_index != WebUI::GBA_STREAM_COUNT; ++gba_index)
    {
      const auto gba_path = fmt::format("/gba{}", gba_index + 1);
      m_server.WebSocket(gba_path, [this, gba_index](const httplib::Request& request,
                                                     httplib::ws::WebSocket& web_socket) {
        // FYI: These requests each happen on their own thread.
        Common::SetCurrentThreadName(fmt::format("WebSocket GBA{}", gba_index + 1).c_str());

        TrackWebSocketAndRunHandler(web_socket, GBAWebSocket{gba_index, web_socket}, request,
                                    m_gba_av_streams[gba_index]);
      });
    }

    m_server.set_logger([](const httplib::Request& req, const httplib::Response& res) {
      if (res.status == 200)
        DEBUG_LOG_FMT(COMMON, "WebUI: {} {} -> {}", req.method, req.path, res.status);
      else
        INFO_LOG_FMT(COMMON, "WebUI: {} {} -> {}", req.method, req.path, res.status);
    });

    const bool bind_successful = m_server.bind_to_port(bind_ip, bind_port);
    if (!bind_successful)
    {
      ERROR_LOG_FMT(COMMON, "WebServer: bind_to_port: {}:{}", bind_ip, bind_port);
      return;
    }

    GetServerEvents().server_started.Trigger();

    NOTICE_LOG_FMT(COMMON, "WebUI: Server started: {}:{}", bind_ip, bind_port);

    m_server.listen_after_bind();

    NOTICE_LOG_FMT(COMMON, "WebUI: Server stopped");
  }

  // Open WebSocket are tracked so destruction can close them all, triggering threads to finish.
  template <typename... Args>
  void TrackWebSocketAndRunHandler(httplib::ws::WebSocket& web_socket, auto&& handler,
                                   Args&&... handler_args)
  {
    {
      std::lock_guard lk{m_web_socket_mutex};
      m_web_sockets.emplace(&web_socket);
    }

    handler(std::forward<Args>(handler_args)...);

    std::lock_guard lk{m_web_socket_mutex};
    m_web_sockets.erase(&web_socket);
  }

  std::array<WeakInstance<AVStream>, WebUI::GBA_STREAM_COUNT> m_gba_av_streams{};

  httplib::Server m_server;
  std::mutex m_web_socket_mutex;
  std::set<httplib::ws::WebSocket*> m_web_sockets;
  std::thread m_thread;
};

WebServer::WebServer(std::string listen_ip_port)
    : m_impl{std::make_unique<Impl>(std::move(listen_ip_port))}
{
}

WebServer::~WebServer() = default;

std::weak_ptr<AVStream> WebServer::GetGBAStream(std::size_t gba_index)
{
  return m_impl->m_gba_av_streams[gba_index].Get();
}

ServerEvents& GetServerEvents()
{
  static ServerEvents s_server_events;
  return s_server_events;
}

void RefreshConfig()
{
  if (Config::Get(Config::MAIN_ENABLE_WEB_INTERFACE))
    StartServer();
  else
    StopServer();
}

}  // namespace WebUI
