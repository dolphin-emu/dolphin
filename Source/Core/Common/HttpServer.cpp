// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/HttpServer.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "Common/Logging/Log.h"
#include "Common/Network.h"
#include "Common/ScopeGuard.h"
#include "Common/SocketContext.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/TimeUtil.h"
#include "Common/Version.h"

#ifndef _WIN32
#define closesocket close
#endif

namespace Common
{
HttpServer::HttpServer(IPv4Port address, int max_clients)
    : m_address(address), m_max_clients(max_clients),
      m_server_ready_future(m_server_ready_promise.get_future().share())
{
}

HttpServer::~HttpServer()
{
  Stop();
}

void HttpServer::ServePath(const std::string& path, const HttpRequestCallback& callback)
{
  std::lock_guard lock(m_routes_mutex);
  m_routes[path] = callback;
}

void HttpServer::Start()
{
  if (m_server_thread.joinable())
    return;

  m_stop_flag.store(false);
  m_server_thread = std::thread([this]() { Serve(); });

  if (m_server_ready_future.get() != ServerReadyState::OK)
  {
    ERROR_LOG_FMT(COMMON, "HttpServer: Failed to start the server");
    Stop();
  }
}

void HttpServer::Stop()
{
  if (!m_server_thread.joinable())
    return;

  m_stop_flag.store(true);
  const s32 server_sock = m_server_sock.exchange(-1);
  if (server_sock >= 0)
  {
#ifndef _WIN32
    shutdown(server_sock, SHUT_RDWR);
#endif
    closesocket(server_sock);
  }
  m_server_thread.join();

  std::vector<std::thread> worker_threads;
  std::vector<s32> worker_socks;
  {
    std::lock_guard lock(m_workers_mutex);
    worker_threads = std::move(m_workers);
    worker_socks = std::move(m_worker_socks);
  }
  for (auto sock : worker_socks)
  {
#ifndef _WIN32
    shutdown(sock, SHUT_RDWR);
#endif
    closesocket(sock);
  }
  for (auto& worker : worker_threads)
  {
    if (worker.joinable())
      worker.join();
  }
  {
    std::lock_guard lock(m_workers_mutex);
    m_workers.clear();
    m_worker_socks.clear();
  }
}

void HttpServer::Serve()
{
  Common::SetCurrentThreadName("Http Server Thread");
  Common::SocketContext socket_context;
  const s32 server_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (server_sock < 0)
  {
    ERROR_LOG_FMT(COMMON, "HttpServer: Could not create a socket for the server - {}",
                  Common::StrNetworkError());
    m_server_ready_promise.set_value(ServerReadyState::Failed);
    return;
  }

#ifndef _WIN32
  int optval = 1;
  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
  {
    WARN_LOG_FMT(COMMON, "HttpServer: Failed to set SO_REUSEADDR - {}", Common::StrNetworkError());
  }
#ifdef SO_REUSEPORT
  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0)
  {
    WARN_LOG_FMT(COMMON, "HttpServer: Failed to set SO_REUSEPORT - {}", Common::StrNetworkError());
  }
#endif
#endif

  if (!Common::SetPlatformSocketOptions(server_sock))
    return;
  m_server_sock.store(server_sock);
  Common::ScopeGuard guard([this] {
    const s32 sock = m_server_sock.exchange(-1);
    if (sock >= 0)
      closesocket(sock);
  });

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = m_address.port;
  server_addr.sin_addr = std::bit_cast<in_addr>(m_address.ip_address);

  if (bind(server_sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0)
  {
    ERROR_LOG_FMT(COMMON, "HttpServer: Could not bind to {} - {}", m_address.ToString(),
                  Common::StrNetworkError());
    m_server_ready_promise.set_value(ServerReadyState::Failed);
    return;
  }

  if (m_address.GetPortValue() == 0)
  {
    sockaddr_in actual_addr{};
    socklen_t addr_len = sizeof(actual_addr);
    if (getsockname(server_sock, reinterpret_cast<sockaddr*>(&actual_addr), &addr_len) < 0)
    {
      ERROR_LOG_FMT(COMMON, "HttpServer: getsockname failed - {}", Common::StrNetworkError());
      m_server_ready_promise.set_value(ServerReadyState::Failed);
      return;
    }

    m_address.port = actual_addr.sin_port;
  }

  if (listen(server_sock, m_max_clients) < 0)
  {
    ERROR_LOG_FMT(COMMON, "HttpServer: Failed to listen to server socket - {}",
                  Common::StrNetworkError());
    m_server_ready_promise.set_value(ServerReadyState::Failed);
    return;
  }

  m_server_ready_promise.set_value(ServerReadyState::OK);

  INFO_LOG_FMT(COMMON, "HttpServer: Running on {}", m_address.ToString());

  while (!m_stop_flag.load())
  {
    sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);
    const s32 client_sock =
        accept(server_sock, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
    if (client_sock < 0)
    {
      if (!m_stop_flag.load())
      {
        ERROR_LOG_FMT(COMMON, "HttpServer: Failed to accept connection from a client - {}",
                      Common::StrNetworkError());
      }
      continue;
    }
    if (!Common::SetPlatformSocketOptions(client_sock))
      continue;
    {
      std::lock_guard lock(m_workers_mutex);
      m_worker_socks.push_back(client_sock);
      m_workers.emplace_back([this, client_sock]() { HandleClient(client_sock); });
    }
  }
}

void HttpServer::HandleClient(s32 client_sock)
{
  Common::SetCurrentThreadName("Http Server Worker Thread");
  Common::ScopeGuard guard([this, client_sock]() {
    std::lock_guard lk(m_workers_mutex);
    auto it = std::find(m_worker_socks.begin(), m_worker_socks.end(), client_sock);
    if (it != m_worker_socks.end())
    {
      m_worker_socks.erase(it);
      closesocket(client_sock);
    }
  });

  std::string request;
  char buffer[1024];

  constexpr int MAX_REQUEST_SIZE = 8192;
  constexpr int MAX_ITERATIONS = 100;
  int iterations = 0;

  while (iterations++ < MAX_ITERATIONS)
  {
    const int bytes = recv(client_sock, buffer, sizeof(buffer), 0);
    if (bytes == 0)
      return;

    if (bytes < 0)
    {
      ERROR_LOG_FMT(COMMON, "HttpServer: Failed to receive data - {}", Common::StrNetworkError());
      return;
    }

    request.append(buffer, bytes);

    if (request.find("\r\n\r\n") != std::string::npos)
      break;

    if (request.size() > MAX_REQUEST_SIZE)
      return;
  }

  const auto first_line_end = request.find("\r\n");
  if (first_line_end == std::string_view::npos)
    return;

  const auto parts = SplitStringIntoArray<3>({request.data(), first_line_end}, ' ');
  if (!parts)
    return;

  const std::string_view method = (*parts)[0];
  const std::string_view path = (*parts)[1];
  const std::string_view version = (*parts)[2];

  const auto local_time = Common::LocalTime(std::time(nullptr));
  if (!local_time)
    return;

  const std::string date = fmt::format("{:%a, %d %b %Y %H:%M:%S GMT}", *local_time);

  if (version != "HTTP/1.0" && version != "HTTP/1.1")
  {
    const std::string response = fmt::format(
        "HTTP/1.0 505 HTTP Version Not Supported\r\nDate: {}\r\nContent-Length: 0\r\n\r\n", date);
    if (send(client_sock, response.c_str(), static_cast<int>(response.size()), 0) < 0)
    {
      ERROR_LOG_FMT(COMMON, "HttpServer: Failed to send response - {}", Common::StrNetworkError());
      return;
    }
    return;
  }

  const bool is_get = (method == "GET");
  const bool is_head = (method == "HEAD");

  if (!is_get && !is_head)
  {
    const std::string response =
        fmt::format("HTTP/1.0 501 Not Implemented\r\nDate: {}\r\nContent-Length: 0\r\n\r\n", date);
    if (send(client_sock, response.c_str(), static_cast<int>(response.size()), 0) < 0)
    {
      ERROR_LOG_FMT(COMMON, "HttpServer: Failed to send response - {}", Common::StrNetworkError());
      return;
    }
    return;
  }

  std::function<HttpResponse()> callback;
  {
    std::lock_guard<std::mutex> lock(m_routes_mutex);

    const auto it = m_routes.find(path);
    if (it == m_routes.end())
    {
      const std::string response =
          fmt::format("HTTP/1.0 404 Not Found\r\nDate: {}\r\nContent-Length: 0\r\n\r\n", date);
      if (send(client_sock, response.c_str(), static_cast<int>(response.size()), 0) < 0)
      {
        ERROR_LOG_FMT(COMMON, "HttpServer: Failed to send response - {}",
                      Common::StrNetworkError());
        return;
      }
      return;
    }

    callback = it->second;
  }
  const HttpResponse response = callback();

  std::string headers;
  for (const auto& header : response.first)
    headers += fmt::format("{}: {}\r\n", header.first, header.second);
  const std::vector<u8>& content = response.second;

  const std::string http_header =
      fmt::format("HTTP/1.0 200 OK\r\n"
                  "Server: {}\r\n"
                  "Date: {}\r\n"
                  "Content-Length: {}\r\n"
                  "{}\r\n",
                  Common::GetHttpServerStr(), date, content.size(), headers);

  std::vector<u8> send_buffer;
  send_buffer.reserve(http_header.size() + content.size());
  send_buffer.insert(send_buffer.end(), http_header.begin(), http_header.end());
  if (!is_head)
    send_buffer.insert(send_buffer.end(), content.begin(), content.end());

  for (std::size_t sent = 0; sent < send_buffer.size();)
  {
    const int bytes = send(client_sock, reinterpret_cast<const char*>(send_buffer.data() + sent),
                           static_cast<int>(send_buffer.size() - sent), 0);
    if (bytes < 0)
    {
      ERROR_LOG_FMT(COMMON, "HttpServer: Failed to send response - {}", Common::StrNetworkError());
      return;
    }
    sent += bytes;
  }
}

IPv4Port HttpServer::GetAddress() const
{
  return m_address;
}

}  // namespace Common
