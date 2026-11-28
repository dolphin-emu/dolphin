// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/NetPlayDiscovery.h"

#include <random>
#include <utility>

#include <fmt/format.h>

#include "Common/Logging/Log.h"
#include "Common/MulticastDNS.h"
#include "Common/StringUtil.h"

namespace
{
constexpr const char* const SERVICE_NAME = "_DolphinNetPlay._udp.local";

// Client periodically sends a query:
// PTR <SERVICE_NAME>

// Server replies and sends resource records on startup:
// PTR <SERVICE_NAME> -> <UID>.<SERVICE_NAME>
// TXT <UID>.<SERVICE_NAME> -> data

// Server sends the the PTR record with a TTL of 0 on shutdown.

constexpr DT SERVER_TTL = std::chrono::seconds{5};

// The client adjusts query timing based on the TTL expiry of observed servers.
// Clients attempt to somewhat cooperate by not spamming queries unnecessarily.
constexpr DT QUERY_MIN_DELAY = std::chrono::seconds{1};
constexpr DT QUERY_MAX_DELAY = std::chrono::seconds{5};
constexpr DT QUERY_MAX_JITTER = std::chrono::milliseconds{500};

constexpr DT RECV_TIMEOUT = std::chrono::milliseconds{500};

constexpr std::string_view FIELD_SERVER_NAME = "server_name";
constexpr std::string_view FIELD_VERSION = "version";
constexpr std::string_view FIELD_GAME_NAME = "game_name";
constexpr std::string_view FIELD_PLAYERS = "players";
constexpr std::string_view FIELD_IN_GAME = "in_game";
constexpr std::string_view FIELD_PORT = "port";
constexpr std::string_view FIELD_PLATFORM = "platform";

// TODO: Make the real TryParse take string_view.
bool TryParse(std::string_view str, auto* output)
{
  return ::TryParse(std::string(str), output);
}

std::string GenerateUniqueID()
{
  std::random_device rng;
  const auto rng_value = std::uniform_int_distribution{u64{}, u64(-1)}(rng);
  const auto time_value = u16(std::chrono::nanoseconds{Clock::now().time_since_epoch()}.count());
  return fmt::format("{:16x}{:04x}", rng_value, time_value);
}

std::vector<u8>
BuildResourceRecordPacket(std::string_view unique_id,
                          const NetPlay::Discovery::DiscoveredServerDetails& details)
{
  using namespace MulticastDNS;

  const auto encoded_unique_name =
      EncodeResourceName(fmt::format("{}.{}", unique_id, SERVICE_NAME));

  const u32 ttl = duration_cast<std::chrono::seconds>(SERVER_TTL).count();

  PacketBuilder builder;

  builder.AddAnswerRecord(PTRRecordData{
      .resource_name = EncodeResourceName(SERVICE_NAME),
      .ttl = ttl,
      .target = encoded_unique_name,
  });

  std::vector<u8> txt_data;
  txt_data.reserve(1024);

  const auto append_field = [&](std::string_view key_str, auto&& value) {
    AppendEncodedString(&txt_data, fmt::format("{}={}", key_str, value));
  };

  append_field(FIELD_SERVER_NAME, details.server_name);
  append_field(FIELD_VERSION, details.version);
  append_field(FIELD_GAME_NAME, details.game_name);
  append_field(FIELD_PLAYERS, int(details.player_count));
  append_field(FIELD_IN_GAME, int(details.in_game));
  append_field(FIELD_PORT, details.port);
  append_field(FIELD_PLATFORM, details.platform);

  builder.AddAdditionalRecord(TXTRecordData{
      .resource_name = encoded_unique_name,
      .ttl = ttl,
      .data = txt_data,
  });

  const u16 flags = 0x0001;  // Response.
  return builder.GetData(0, flags);
}

std::vector<u8> BuildShutdownPacket(std::string_view unique_id)
{
  using namespace MulticastDNS;

  PacketBuilder builder;

  builder.AddAnswerRecord(PTRRecordData{
      .resource_name = EncodeResourceName(SERVICE_NAME),
      .ttl = 0,
      .target = EncodeResourceName(fmt::format("{}.{}", unique_id, SERVICE_NAME)),
  });

  const u16 flags = 0x0001;  // Response.
  return builder.GetData(0, flags);
}

}  // namespace

namespace NetPlay::Discovery
{

Server::~Server()
{
  Stop();
}

void Server::Start()
{
  if (m_running.exchange(true, std::memory_order_relaxed))
    return;

  m_thread = std::thread(&Server::ThreadFunc, this);

  INFO_LOG_FMT(NETPLAY, "Discovery server started.");
}

void Server::Stop()
{
  if (!m_running.exchange(false, std::memory_order_relaxed))
    return;

  if (m_thread.joinable())
    m_thread.join();

  INFO_LOG_FMT(NETPLAY, "Discovery server stopped.");
}

void Server::Update(DiscoveredServerDetails details)
{
  // TODO: Ideally this would kick the thread to send updated data.
  std::lock_guard lock(m_details_mutex);
  m_details = std::move(details);
}

void Server::ThreadFunc()
{
  using namespace MulticastDNS;

  QuerySocket socket;
  if (!socket.IsOpen())
    return;

  // A unique identifier for this session.
  const std::string unique_id = GenerateUniqueID();

  const auto send_records = [&] {
    std::vector<u8> packet;

    {
      std::lock_guard lg(m_details_mutex);
      packet = BuildResourceRecordPacket(unique_id, m_details);
    }

    DEBUG_LOG_FMT(NETPLAY, "Sending mDNS records.");

    socket.Send(packet);
  };

  // Send on startup to immediately populate existing clients.
  send_records();

  while (m_running.load(std::memory_order_relaxed))
  {
    Common::IPAddress from_ip{};
    Common::UniqueBuffer<u8> buffer;
    const int received = socket.ReceiveFrom(RECV_TIMEOUT, &buffer, &from_ip);

    if (received <= 0)
      continue;

    DecodePacket(
        std::span{buffer}.first(received),
        [&](QueryData query) {
          if (DecodeResourceName(query.resource_name) != SERVICE_NAME)
            return;

          DEBUG_LOG_FMT(NETPLAY, "Received mDNS query.");

          // Send records to any queries for our service name.
          send_records();
        },
        [&](RecordCategory, GenericRecordData) {
          // We don't care about records.
        });
  }

  // Inform clients of our going away.
  socket.Send(BuildShutdownPacket(unique_id));
}

Client::~Client()
{
  Stop();
}

void Client::Start()
{
  if (m_running.exchange(true, std::memory_order_relaxed))
    return;

  m_thread = std::thread(&Client::ThreadFunc, this);

  INFO_LOG_FMT(NETPLAY, "Discovery client started.");
}

void Client::Stop()
{
  if (!m_running.exchange(false, std::memory_order_relaxed))
    return;

  if (m_thread.joinable())
    m_thread.join();

  INFO_LOG_FMT(NETPLAY, "Discovery client stopped.");
}

std::vector<DiscoveredServer> Client::GetServers() const
{
  std::lock_guard lg(m_servers_mutex);
  std::vector<DiscoveredServer> result;
  result.reserve(m_servers.size());
  for (const auto& [uid, server] : m_servers)
  {
    if (server.details.player_count != 0)
      result.push_back(server);
  }
  return result;
}

void Client::ThreadFunc()
{
  using namespace MulticastDNS;

  QuerySocket socket;
  if (!socket.IsOpen())
    return;

  const auto send_query = [&] {
    PacketBuilder builder;
    builder.AddQuestion({
        .resource_name = EncodeResourceName(SERVICE_NAME),
        .record_type = RecordType::ANY,
    });

    DEBUG_LOG_FMT(NETPLAY, "Sending mDNS query.");

    u16 flags = 0x0000;  // Query.
    socket.Send(builder.GetData(0, flags));

    UpdateLastQueryTime();
  };

  send_query();

  while (m_running.load(std::memory_order_relaxed))
  {
    PruneStaleServers();

    const auto now = Clock::now();

    DT next_query_delay = QUERY_MAX_DELAY;

    {
      std::lock_guard lk{m_servers_mutex};
      for (const auto& [uid, server] : m_servers)
        next_query_delay = std::min(next_query_delay, (server.expiry_time - now) / 2);
    }

    next_query_delay = std::max(next_query_delay, QUERY_MIN_DELAY);

    const auto next_query_time = m_last_query_time + next_query_delay;

    if (now >= next_query_time)
      send_query();

    Common::IPAddress from_ip{};
    Common::UniqueBuffer<u8> buffer;
    const int received = socket.ReceiveFrom(RECV_TIMEOUT, &buffer, &from_ip);

    if (received <= 0)
      continue;

    DiscoveredServer discovered;
    discovered.address = Common::IPAddressToString(from_ip);

    HandlePacket(std::span{buffer}.first(received), from_ip);
  }

  {
    std::lock_guard lg(m_servers_mutex);
    m_servers.clear();
  }
}

void Client::HandlePacket(std::span<const u8> packet, const Common::IPAddress& from_ip)
{
  using namespace MulticastDNS;

  const auto now = Clock::now();

  const auto update_server_expiry = [&](DiscoveredServer& server, u32 ttl) {
    // Limit the allowed TTL.
    server.expiry_time = now + std::min<DT>(SERVER_TTL, std::chrono::seconds{ttl});
  };

  DecodePacket(
      packet,
      [&](QueryData query) {
        if (DecodeResourceName(query.resource_name) != SERVICE_NAME)
          return;

        DEBUG_LOG_FMT(NETPLAY, "Observed mDNS query.");

        UpdateLastQueryTime();
      },
      [&](RecordCategory, GenericRecordData record) {
        if (auto ptr_record = PTRRecordData::TryParse(record))
        {
          if (DecodeResourceName(ptr_record.resource_name) != SERVICE_NAME)
            return;

          DEBUG_LOG_FMT(NETPLAY, "Received mDNS PTR record.");

          // FYI: this also includes the .<SERVICE_NAME> bit, but that's okay.
          const auto discovered_uid = DecodeResourceName(ptr_record.target);
          if (!discovered_uid)
            return;  // Failed to parse.

          std::lock_guard lg(m_servers_mutex);
          const auto [it, inserted] = m_servers.try_emplace(*discovered_uid, DiscoveredServer{});
          auto& server = it->second;

          // FYI: Rather than having the server figure out the listening IP,
          //  we currently just use the IP of the PTR record source.
          // This is a bit janky, but it will work for now for people with one local IP.
          // A more compliant implementation might use an A/AAAA record.
          server.address = Common::IPAddressToString(from_ip);

          if (inserted)
            INFO_LOG_FMT(NETPLAY, "Discovered new server: {}", server.address);

          update_server_expiry(server, ptr_record.ttl);
        }
        else if (auto txt_record = TXTRecordData::TryParse(record))
        {
          // FYI: this also includes the .<SERVICE_NAME> bit, but that's okay.
          const auto discovered_uid = DecodeResourceName(txt_record.resource_name);
          if (!discovered_uid)
            return;  // Failed to parse.

          std::lock_guard lg(m_servers_mutex);
          const auto it = m_servers.find(*discovered_uid);
          if (it == m_servers.end())
            return;  // We don't know about this server.

          DEBUG_LOG_FMT(NETPLAY, "Received mDNS TXT record.");

          auto& server = it->second;
          auto& details = server.details;

          update_server_expiry(server, txt_record.ttl);

          std::string_view key_value;
          while (ParseVariableLengthString(&txt_record.data, &key_value))
          {
            const auto [key, value] = SplitKeyValuePair(key_value);
            if (key == FIELD_SERVER_NAME)
              details.server_name = value;
            else if (key == FIELD_VERSION)
              details.version = value;
            else if (key == FIELD_GAME_NAME)
              details.game_name = value;
            else if (key == FIELD_PLAYERS)
              TryParse(value, &details.player_count);
            else if (key == FIELD_IN_GAME)
              TryParse(value, &details.in_game);
            else if (key == FIELD_PORT)
              TryParse(value, &details.port);
            else if (key == FIELD_PLATFORM)
              details.platform = value;
          }
        }
      });
}

void Client::UpdateLastQueryTime()
{
  std::random_device rng;
  const DT jitter{std::uniform_int_distribution(DT{}.count(), QUERY_MAX_JITTER.count())(rng)};

  m_last_query_time = Clock::now() + jitter;
}

void Client::PruneStaleServers()
{
  const auto now = Clock::now();
  std::lock_guard lg(m_servers_mutex);
  std::erase_if(m_servers, [&](const auto& entry) { return now >= entry.second.expiry_time; });
}

}  // namespace NetPlay::Discovery
