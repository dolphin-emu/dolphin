// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <future>
#include <map>
#include <thread>

#include "Common/CommonTypes.h"
#include "Common/Network.h"

namespace Common
{
using HttpHeaders = std::map<const std::string, const std::string>;
using HttpResponse = std::pair<const HttpHeaders, const std::vector<u8>>;
using HttpRequestCallback = std::function<const HttpResponse()>;

class HttpServer final
{
public:
  enum class ServerReadyState : int
  {
    OK,
    Failed,
  };

  explicit HttpServer(IPv4Port address, int max_clients = 10);
  ~HttpServer();

  void ServePath(const std::string& path, const HttpRequestCallback& callback);

  void Start();
  void Stop();

  IPv4Port GetAddress() const;

private:
  IPv4Port m_address;
  const int m_max_clients;
  std::map<std::string, HttpRequestCallback, std::less<>> m_routes;
  std::mutex m_routes_mutex;
  std::atomic<bool> m_stop_flag{false};
  std::thread m_server_thread;
  std::atomic<s32> m_server_sock{-1};
  std::promise<ServerReadyState> m_server_ready_promise;
  std::shared_future<ServerReadyState> m_server_ready_future;
  std::vector<std::thread> m_workers;
  std::vector<s32> m_worker_socks;
  std::mutex m_workers_mutex;

  void Serve();
  void HandleClient(s32 client_sock);
};
}  // namespace Common
