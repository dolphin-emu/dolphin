// SPDX-License-Identifier: CC0-1.0

// The central server implementation.
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <tuple>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/format.h>

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "Common/Random.h"
#include "Common/TraversalProto.h"

#define DEBUG 0
#define NUMBER_OF_TRIES 5
#define PORT 6262
#define PORT_ALT 6226

static u64 currentTime;

struct OutgoingPacketInfo
{
  Common::TraversalPacket packet;
  Common::TraversalRequestId misc;
  bool fromAlt;
  sockaddr_in6 dest;
  int tries;
  u64 sendTime;
};

template <typename T>
struct EvictEntry
{
  u64 updateTime;
  T value;
};

template <typename V>
struct EvictFindResult
{
  bool found;
  V* value;
};

template <typename K, typename V>
EvictFindResult<V> EvictFind(std::unordered_map<K, EvictEntry<V>>& map, const K& key,
                             bool refresh = false)
{
retry:
  const u64 expiryTime = 30 * 1000000;  // 30s
  EvictFindResult<V> result;
  if (map.bucket_count())
  {
    auto bucket = map.bucket(key);
    auto it = map.begin(bucket);
    for (; it != map.end(bucket); ++it)
    {
      if (currentTime - it->second.updateTime > expiryTime)
      {
        map.erase(it->first);
        goto retry;
      }
      if (it->first == key)
      {
        if (refresh)
          it->second.updateTime = currentTime;
        result.found = true;
        result.value = &it->second.value;
        return result;
      }
    }
  }
#if DEBUG
  fmt::print("failed to find key '");
  for (size_t i = 0; i < sizeof(key); i++)
  {
    fmt::print("{:02x}", ((u8*)&key)[i]);
  }
  fmt::print("'\n");
#endif
  result.found = false;
  return result;
}

template <typename K, typename V>
V* EvictSet(std::unordered_map<K, EvictEntry<V>>& map, const K& key)
{
  // can't use a local_iterator to emplace...
  auto& result = map[key];
  result.updateTime = currentTime;
  return &result.value;
}

namespace std
{
template <>
struct hash<Common::TraversalHostId>
{
  size_t operator()(const Common::TraversalHostId& id) const noexcept
  {
    auto p = (u32*)id.data();
    return p[0] ^ ((p[1] << 13) | (p[1] >> 19));
  }
};
}  // namespace std

using ConnectedClients =
    std::unordered_map<Common::TraversalHostId, EvictEntry<Common::TraversalInetAddress>>;
using OutgoingPackets = std::unordered_map<Common::TraversalRequestId, OutgoingPacketInfo>;

static int sock;
static int sockAlt;
static OutgoingPackets outgoingPackets;
static ConnectedClients connectedClients;

static Common::TraversalInetAddress MakeInetAddress(const sockaddr_in6& addr)
{
  if (addr.sin6_family != AF_INET6)
  {
    fmt::print(stderr, "bad sockaddr_in6\n");
    exit(1);
  }
  u32* words = (u32*)addr.sin6_addr.s6_addr;
  Common::TraversalInetAddress result = {};
  if (words[0] == 0 && words[1] == 0 && words[2] == 0xffff0000)
  {
    result.isIPV6 = false;
    result.address[0] = words[3];
  }
  else
  {
    result.isIPV6 = true;
    memcpy(result.address, words, sizeof(result.address));
  }
  result.port = addr.sin6_port;
  return result;
}

static sockaddr_in6 MakeSinAddr(const Common::TraversalInetAddress& addr)
{
  sockaddr_in6 result;
#ifdef SIN6_LEN
  result.sin6_len = sizeof(result);
#endif
  result.sin6_family = AF_INET6;
  result.sin6_port = addr.port;
  result.sin6_flowinfo = 0;
  if (addr.isIPV6)
  {
    memcpy(&result.sin6_addr, addr.address, 16);
  }
  else
  {
    u32* words = (u32*)result.sin6_addr.s6_addr;
    words[0] = 0;
    words[1] = 0;
    words[2] = 0xffff0000;
    words[3] = addr.address[0];
  }
  result.sin6_scope_id = 0;
  return result;
}

static void GetRandomHostId(Common::TraversalHostId* hostId)
{
  char buf[9]{};
  const u32 num = Common::Random::GenerateValue<u32>();
  fmt::format_to_n(buf, sizeof(buf) - 1, "{:08x}", num);
  memcpy(hostId->data(), buf, 8);
}

static const char* SenderName(sockaddr_in6* addr)
{
  static char buf[INET6_ADDRSTRLEN + 10]{};
  inet_ntop(PF_INET6, &addr->sin6_addr, buf, sizeof(buf));
  fmt::format_to(buf + strlen(buf), ":{}", ntohs(addr->sin6_port));
  return buf;
}

static void TrySend(const void* buffer, size_t size, sockaddr_in6* addr, bool fromAlt)
{
#if DEBUG
  const auto* packet = static_cast<const Common::TraversalPacket*>(buffer);
  fmt::print("{}-> {} {} {}\n", fromAlt ? "alt " : "", static_cast<int>(packet->type),
             static_cast<long long>(packet->requestId), SenderName(addr));
#endif
  if ((size_t)sendto(fromAlt ? sockAlt : sock, buffer, size, 0, (sockaddr*)addr, sizeof(*addr)) !=
      size)
  {
    perror("sendto");
  }
}

static Common::TraversalPacket* AllocPacket(const sockaddr_in6& dest, bool fromAlt,
                                            Common::TraversalRequestId misc = 0)
{
  Common::TraversalRequestId requestId{};
  Common::Random::Generate(&requestId, sizeof(requestId));
  OutgoingPacketInfo* info = &outgoingPackets[requestId];
  info->fromAlt = fromAlt;
  info->dest = dest;
  info->misc = misc;
  info->tries = 0;
  info->sendTime = currentTime;
  Common::TraversalPacket* result = &info->packet;
  memset(result, 0, sizeof(*result));
  result->requestId = requestId;
  return result;
}

static void SendPacket(OutgoingPacketInfo* info)
{
  info->tries++;
  info->sendTime = currentTime;
  TrySend(&info->packet, sizeof(info->packet), &info->dest, info->fromAlt);
}

static void ResendPackets()
{
  std::vector<std::tuple<Common::TraversalInetAddress, bool, Common::TraversalRequestId>>
      todoFailures;
  todoFailures.clear();
  for (auto it = outgoingPackets.begin(); it != outgoingPackets.end();)
  {
    OutgoingPacketInfo* info = &it->second;
    if (currentTime - info->sendTime >= (u64)(300000 * info->tries))
    {
      if (info->tries >= NUMBER_OF_TRIES)
      {
        if (info->packet.type == Common::TraversalPacketType::PleaseSendPacket)
        {
          todoFailures.push_back(
              std::make_tuple(info->packet.pleaseSendPacket.address, info->fromAlt, info->misc));
        }
        it = outgoingPackets.erase(it);
        continue;
      }
      else
      {
        SendPacket(info);
      }
    }
    ++it;
  }

  for (const auto& p : todoFailures)
  {
    Common::TraversalPacket* fail = AllocPacket(MakeSinAddr(std::get<0>(p)), std::get<1>(p));
    fail->type = Common::TraversalPacketType::ConnectFailed;
    fail->connectFailed.requestId = std::get<2>(p);
    fail->connectFailed.reason = Common::TraversalConnectFailedReason::ClientDidntRespond;
  }
}

static void HandlePacket(Common::TraversalPacket* packet, sockaddr_in6* addr, bool toAlt)
{
#if DEBUG
  fmt::print("<- {} {} {}\n", static_cast<int>(packet->type),
             static_cast<long long>(packet->requestId), SenderName(addr));
#endif
  bool packetOk = true;
  switch (packet->type)
  {
  case Common::TraversalPacketType::Ack:
  {
    auto it = outgoingPackets.find(packet->requestId);
    if (it == outgoingPackets.end())
      break;

    OutgoingPacketInfo* info = &it->second;

    if (info->packet.type == Common::TraversalPacketType::PleaseSendPacket)
    {
      auto* ready = AllocPacket(MakeSinAddr(info->packet.pleaseSendPacket.address), toAlt);
      if (packet->ack.ok)
      {
        ready->type = Common::TraversalPacketType::ConnectReady;
        ready->connectReady.requestId = info->misc;
        ready->connectReady.address = MakeInetAddress(info->dest);
      }
      else
      {
        ready->type = Common::TraversalPacketType::ConnectFailed;
        ready->connectFailed.requestId = info->misc;
        ready->connectFailed.reason = Common::TraversalConnectFailedReason::ClientFailure;
      }
    }

    outgoingPackets.erase(it);
    break;
  }
  case Common::TraversalPacketType::Ping:
  {
    auto r = EvictFind(connectedClients, packet->ping.hostId, true);
    packetOk = r.found;
    break;
  }
  case Common::TraversalPacketType::HelloFromClient:
  {
    u8 ok = packet->helloFromClient.protoVersion <= Common::TraversalProtoVersion;
    Common::TraversalPacket* reply = AllocPacket(*addr, toAlt);
    reply->type = Common::TraversalPacketType::HelloFromServer;
    reply->helloFromServer.ok = ok;
    if (ok)
    {
      Common::TraversalHostId hostId{};
      Common::TraversalInetAddress* iaddr{};
      // not that there is any significant change of
      // duplication, but...
      while (true)
      {
        GetRandomHostId(&hostId);
        auto r = EvictFind(connectedClients, hostId);
        if (!r.found)
        {
          iaddr = EvictSet(connectedClients, hostId);
          break;
        }
      }

      *iaddr = MakeInetAddress(*addr);

      reply->helloFromServer.yourAddress = *iaddr;
      reply->helloFromServer.yourHostId = hostId;
    }
    break;
  }
  case Common::TraversalPacketType::ConnectPlease:
  {
    Common::TraversalHostId& hostId = packet->connectPlease.hostId;
    auto r = EvictFind(connectedClients, hostId);
    if (!r.found)
    {
      Common::TraversalPacket* reply = AllocPacket(*addr, toAlt);
      reply->type = Common::TraversalPacketType::ConnectFailed;
      reply->connectFailed.requestId = packet->requestId;
      reply->connectFailed.reason = Common::TraversalConnectFailedReason::NoSuchClient;
    }
    else
    {
      Common::TraversalPacket* please =
          AllocPacket(MakeSinAddr(*r.value), toAlt, packet->requestId);
      please->type = Common::TraversalPacketType::PleaseSendPacket;
      please->pleaseSendPacket.address = MakeInetAddress(*addr);
    }
    break;
  }
  case Common::TraversalPacketType::TestPlease:
  {
    Common::TraversalHostId& hostId = packet->testPlease.hostId;
    auto r = EvictFind(connectedClients, hostId);
    if (r.found)
    {
      Common::TraversalPacket ack = {};
      ack.type = Common::TraversalPacketType::Ack;
      ack.requestId = packet->requestId;
      ack.ack.ok = true;
      sockaddr_in6 mainAddr = MakeSinAddr(*r.value);
      TrySend(&ack, sizeof(ack), &mainAddr, toAlt);
    }
    break;
  }
  default:
    fmt::print(stderr, "received unknown packet type {} from {}\n", static_cast<int>(packet->type),
               SenderName(addr));
    break;
  }
  if (packet->type != Common::TraversalPacketType::Ack)
  {
    Common::TraversalPacket ack = {};
    ack.type = Common::TraversalPacketType::Ack;
    ack.requestId = packet->requestId;
    ack.ack.ok = packetOk;
    TrySend(&ack, sizeof(ack), addr,
            packet->type != Common::TraversalPacketType::TestPlease ? toAlt : !toAlt);
  }
}

int main()
{
  int rv;
  sock = socket(PF_INET6, SOCK_DGRAM, 0);
  if (sock == -1)
  {
    perror("socket");
    return 1;
  }
  sockAlt = socket(PF_INET6, SOCK_DGRAM, 0);
  if (sockAlt == -1)
  {
    perror("socket alt");
    return 1;
  }
  int no = 0;
  rv = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
  if (rv < 0)
  {
    perror("setsockopt IPV6_V6ONLY");
    return 1;
  }
  rv = setsockopt(sockAlt, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
  if (rv < 0)
  {
    perror("setsockopt IPV6_V6ONLY alt");
    return 1;
  }
  in6_addr any = IN6ADDR_ANY_INIT;
  sockaddr_in6 addr;
#ifdef SIN6_LEN
  addr.sin6_len = sizeof(addr);
#endif
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(PORT);
  addr.sin6_flowinfo = 0;
  addr.sin6_addr = any;
  addr.sin6_scope_id = 0;

  rv = bind(sock, (sockaddr*)&addr, sizeof(addr));
  if (rv < 0)
  {
    perror("bind");
    return 1;
  }
  addr.sin6_port = htons(PORT_ALT);
  rv = bind(sockAlt, (sockaddr*)&addr, sizeof(addr));
  if (rv < 0)
  {
    perror("bind alt");
    return 1;
  }

  timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 300000;
  rv = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  if (rv < 0)
  {
    perror("setsockopt SO_RCVTIMEO");
    return 1;
  }
  rv = setsockopt(sockAlt, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  if (rv < 0)
  {
    perror("setsockopt SO_RCVTIMEO alt");
    return 1;
  }

#ifdef HAVE_LIBSYSTEMD
  sd_notifyf(0, "READY=1\nSTATUS=Listening on port %d (alt port: %d)", PORT, PORT_ALT);
#endif

  while (true)
  {
    tv.tv_sec = 0;
    tv.tv_usec = 300000;
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sock, &readSet);
    FD_SET(sockAlt, &readSet);
    rv = select(std::max(sock, sockAlt) + 1, &readSet, nullptr, nullptr, &tv);
    if (rv < 0)
    {
      if (errno != EINTR && errno != EAGAIN)
      {
        perror("recvfrom");
        return 1;
      }
    }

    int recvsock;
    if (FD_ISSET(sock, &readSet))
    {
      recvsock = sock;
    }
    else if (FD_ISSET(sockAlt, &readSet))
    {
      recvsock = sockAlt;
    }
    else
    {
      ResendPackets();
      continue;
    }
    sockaddr_in6 raddr;
    socklen_t addrLen = sizeof(raddr);
    Common::TraversalPacket packet{};
    // note: switch to recvmmsg (yes, mmsg) if this becomes
    // expensive
    rv = recvfrom(recvsock, &packet, sizeof(packet), 0, (sockaddr*)&raddr, &addrLen);
    currentTime = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    if (rv < 0)
    {
      if (errno != EINTR && errno != EAGAIN)
      {
        perror("recvfrom");
        return 1;
      }
    }
    else if ((size_t)rv < sizeof(packet))
    {
      fmt::print(stderr, "received short packet from {}\n", SenderName(&raddr));
    }
    else
    {
      HandlePacket(&packet, &raddr, recvsock == sockAlt);
    }
    ResendPackets();
#ifdef HAVE_LIBSYSTEMD
    sd_notify(0, "WATCHDOG=1");
#endif
  }
}
