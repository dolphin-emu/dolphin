// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"

namespace sf
{
class Packet;
}

namespace NetPlay
{
namespace Discovery
{
enum class Platform : u8
{
  Unknown = 0,
  Linux = 1,
  Windows = 2,
  macOS = 3,
  Android = 4,
  iOS = 5,
  FreeBSD = 6,
  OpenBSD = 7,
  NetBSD = 8,
  DragonFlyBSD = 9,
  Haiku = 10
};

struct Payload
{
  std::string server_name;
  std::string version;
  std::string game_name;
  u8 player_count = 0;
  bool in_game = false;
  u16 port = 0;
  Platform platform = Platform::Unknown;
};

struct DiscoveredServer
{
  Payload payload;
  std::string address;
  std::chrono::steady_clock::time_point last_seen;
};

sf::Packet& operator<<(sf::Packet& packet, const Payload& payload);
sf::Packet& operator>>(sf::Packet& packet, Payload& payload);

static constexpr u16 PORT = 2662;
static constexpr u8 PROTOCOL_VERSION = 1;  // Increase this if the payload definition changes
static constexpr std::array<u8, 8> MAGIC{'D', 'O', 'L', 'P', 'H', 'I', 'N', PROTOCOL_VERSION};
static constexpr std::chrono::seconds SERVER_TIMEOUT{5};

Platform GetCurrentPlatform();

class Server
{
public:
  void Start();
  void Stop();
  void Update(Payload payload);

private:
  void DiscoveryThread();

  std::thread m_thread;
  std::atomic<bool> m_running{false};
  std::mutex m_payload_mutex;
  Payload m_payload;
};

class Client
{
public:
  void Start();
  void Stop();

  std::vector<DiscoveredServer> GetServers() const;

private:
  void ListenThread();
  void PruneStaleServers();

  std::thread m_thread;
  std::atomic<bool> m_running{false};
  mutable std::mutex m_servers_mutex;
  std::unordered_map<std::string, DiscoveredServer> m_servers;
};
}  // namespace Discovery
}  // namespace NetPlay
