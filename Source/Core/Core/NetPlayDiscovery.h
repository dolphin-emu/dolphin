// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Network.h"

namespace NetPlay::Discovery
{

struct DiscoveredServerDetails
{
  std::string server_name;
  std::string version;
  std::string game_name;
  u8 player_count = 0;
  bool in_game = false;
  u16 port = 0;
  std::string platform;
};

struct DiscoveredServer
{
  DiscoveredServerDetails details;
  std::string address;
  TimePoint expiry_time;
};

class Server
{
public:
  ~Server();

  void Start();
  void Stop();

  void Update(DiscoveredServerDetails details);

private:
  void ThreadFunc();

  std::thread m_thread;
  std::atomic<bool> m_running{false};

  std::mutex m_details_mutex;
  DiscoveredServerDetails m_details;
};

class Client
{
public:
  ~Client();

  void Start();
  void Stop();

  std::vector<DiscoveredServer> GetServers() const;

private:
  void ThreadFunc();

  void PruneStaleServers();

  void HandlePacket(std::span<const u8> packet, const Common::IPAddress& from_ip);
  void UpdateLastQueryTime();

  std::thread m_thread;
  std::atomic<bool> m_running{false};
  mutable std::mutex m_servers_mutex;

  // FYI: Adjusted by jitter value.
  TimePoint m_last_query_time{};

  // The key is a unique ID.
  std::unordered_map<std::string, DiscoveredServer> m_servers;
};
}  // namespace NetPlay::Discovery
