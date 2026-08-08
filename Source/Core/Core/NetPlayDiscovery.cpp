// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/NetPlayDiscovery.h"

#include <cstring>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#define closesocket close
#else
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
using socklen_t = int;
#endif
#include <SFML/Network/Packet.hpp>

#include "Common/Logging/Log.h"
#include "Common/Network.h"

namespace NetPlay::Discovery
{
sf::Packet& operator<<(sf::Packet& packet, const Payload& payload)
{
  return packet << payload.server_name << payload.version << payload.game_name
                << payload.player_count << payload.in_game << payload.port
                << static_cast<u8>(payload.platform);
}

sf::Packet& operator>>(sf::Packet& packet, Payload& payload)
{
  u8 platform_raw = 0;
  packet >> payload.server_name >> payload.version >> payload.game_name >> payload.player_count >>
      payload.in_game >> payload.port >> platform_raw;
  payload.platform = static_cast<Platform>(platform_raw);
  return packet;
}

Platform GetCurrentPlatform()
{
#ifdef __ANDROID__
  return Platform::Android;
#elifdef __linux__
  return Platform::Linux;
#elifdef _WIN32
  return Platform::Windows;
#elifdef TARGET_OS_OSX
  return Platform::macOS;
#elifdef TARGET_OS_IOS
  return Platform::iOS;
#elifdef __FreeBSD__
  return Platform::FreeBSD;
#elifdef __OpenBSD__
  return Platform::OpenBSD;
#elifdef __NetBSD__
  return Platform::NetBSD;
#elifdef __DragonFly__
  return Platform::DragonFlyBSD;
#elifdef __HAIKU__
  return Platform::Haiku;
#else
  return Platform::Unknown;
#endif
}

void Server::Start()
{
  if (m_running.exchange(true))
    return;

  m_thread = std::thread(&Server::DiscoveryThread, this);
}

void Server::Stop()
{
  if (!m_running.exchange(false))
    return;

  if (m_thread.joinable())
    m_thread.join();
}

void Server::Update(Payload payload)
{
  std::lock_guard lock(m_payload_mutex);
  m_payload = payload;
}

void Server::DiscoveryThread()
{
  const int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
  {
    ERROR_LOG_FMT(NETPLAY, "NetPlayDiscovery: failed to create broadcast socket.");
    return;
  }

  constexpr int broadcast_enable = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast_enable),
                 sizeof(broadcast_enable)) < 0)
  {
    ERROR_LOG_FMT(COMMON, "Failed to set SO_BROADCAST on socket: {}", Common::StrNetworkError());
  };

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

  INFO_LOG_FMT(NETPLAY, "Discovery broadcast started on port {}", PORT);
  while (m_running)
  {
    sf::Packet packet;
    packet.append(MAGIC.data(), sizeof(MAGIC));

    {
      std::lock_guard lg(m_payload_mutex);
      packet << m_payload;
    }

    sendto(sock, static_cast<const char*>(packet.getData()), static_cast<int>(packet.getDataSize()),
           0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    std::this_thread::sleep_for(SERVER_TIMEOUT / 3);
  }

  INFO_LOG_FMT(NETPLAY, "Discovery broadcast stopped");
  closesocket(sock);
}

void Client::Start()
{
  if (m_running.exchange(true))
    return;

  m_thread = std::thread(&Client::ListenThread, this);
}

void Client::Stop()
{
  if (!m_running.exchange(false))
    return;

  if (m_thread.joinable())
    m_thread.join();
}

std::vector<DiscoveredServer> Client::GetServers() const
{
  std::lock_guard lg(m_servers_mutex);
  std::vector<DiscoveredServer> result;
  result.reserve(m_servers.size());
  for (const auto& [addr, server] : m_servers)
    result.push_back(server);
  return result;
}

void Client::ListenThread()
{
  const int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
  {
    ERROR_LOG_FMT(NETPLAY, "NetPlay discovery client: failed to create socket.");
    return;
  }

  constexpr int reuse = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                 sizeof(reuse)) < 0)
  {
    ERROR_LOG_FMT(COMMON, "Failed to set SO_REUSEADDR on socket: {}", Common::StrNetworkError());
  };

  // Don't block on recvfrom so we can prune stale entries
  constexpr int timeout_ms = 500;
#if defined(_WIN32)
  DWORD timeout = timeout_ms;
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
                 sizeof(timeout)) < 0)
#else
  timeval timeout{.tv_sec = 0, .tv_usec = timeout_ms * 1000};
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
#endif
  {
    ERROR_LOG_FMT(COMMON, "Failed to set SO_RCVTIMEO on socket: {}", Common::StrNetworkError());
  };

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
  {
    ERROR_LOG_FMT(NETPLAY, "NetPlay discovery client: failed to bind to port {}.", PORT);
    closesocket(sock);
    return;
  }

  std::vector<u8> buf(512);

  while (m_running)
  {
    sockaddr_in from{};
    socklen_t from_len = sizeof(from);

    const int received =
        recvfrom(sock, reinterpret_cast<char*>(buf.data()), static_cast<int>(buf.size()), 0,
                 reinterpret_cast<sockaddr*>(&from), &from_len);

    if (received <= 0)
    {
      PruneStaleServers();
      continue;
    }

    if (static_cast<size_t>(received) < MAGIC.size() ||
        std::memcmp(buf.data(), MAGIC.data(), MAGIC.size()) != 0)
    {
      PruneStaleServers();
      continue;
    }

    sf::Packet packet;
    packet.append(buf.data() + MAGIC.size(), received - MAGIC.size());

    Payload payload;
    packet >> payload;

    if (!packet)
      continue;

    char addr_str[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &from.sin_addr, addr_str, sizeof(addr_str));

    DiscoveredServer discovered;
    discovered.payload = std::move(payload);
    discovered.address = addr_str;
    discovered.last_seen = std::chrono::steady_clock::now();

    {
      std::lock_guard lg(m_servers_mutex);
      m_servers[discovered.address] = std::move(discovered);
    }
  }

  closesocket(sock);

  {
    std::lock_guard lg(m_servers_mutex);
    m_servers.clear();
  }
}

void Client::PruneStaleServers()
{
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lg(m_servers_mutex);
  std::erase_if(m_servers, [&](const auto& entry) {
    return now - entry.second.last_seen > SERVER_TIMEOUT || entry.second.payload.player_count == 0;
  });
}
}  // namespace NetPlay::Discovery
