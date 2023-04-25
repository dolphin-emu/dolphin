// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <array>
#include <cstddef>
#include "Common/CommonTypes.h"

namespace Common
{
constexpr size_t NETPLAY_CODE_SIZE = 8;
using TraversalHostId = std::array<char, NETPLAY_CODE_SIZE>;
using TraversalRequestId = u64;

enum class TraversalPacketType : u8
{
  // [*->*]
  Ack = 0,
  // [c->s]
  Ping = 1,
  // [c->s]
  HelloFromClient = 2,
  // [s->c]
  HelloFromServer = 3,
  // [c->s] When connecting, first the client asks the central server...
  ConnectPlease = 4,
  // [s->c] ...who asks the game host to send a UDP packet to the
  // client... (an ack implies success)
  PleaseSendPacket = 5,
  // [s->c] ...which the central server relays back to the client.
  ConnectReady = 6,
  // [s->c] Alternately, the server might not have heard of this host.
  ConnectFailed = 7,
};

constexpr u8 TraversalProtoVersion = 0;

enum class TraversalConnectFailedReason : u8
{
  ClientDidntRespond = 0,
  ClientFailure,
  NoSuchClient,
};

#pragma pack(push, 1)
struct TraversalInetAddress
{
  u8 isIPV6;
  u32 address[4];
  u16 port;
};
struct TraversalPacket
{
  TraversalPacketType type;
  TraversalRequestId requestId;
  union
  {
    struct
    {
      u8 ok;
    } ack;
    struct
    {
      TraversalHostId hostId;
    } ping;
    struct
    {
      u8 protoVersion;
    } helloFromClient;
    struct
    {
      u8 ok;
      TraversalHostId yourHostId;
      TraversalInetAddress yourAddress;
    } helloFromServer;
    struct
    {
      TraversalHostId hostId;
    } connectPlease;
    struct
    {
      TraversalInetAddress address;
    } pleaseSendPacket;
    struct
    {
      TraversalRequestId requestId;
      TraversalInetAddress address;
    } connectReady;
    struct
    {
      TraversalRequestId requestId;
      TraversalConnectFailedReason reason;
    } connectFailed;
  };
};
#pragma pack(pop)
}  // namespace Common
