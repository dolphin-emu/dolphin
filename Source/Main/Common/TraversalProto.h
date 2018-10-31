// This file is public domain, in case it's useful to anyone. -comex

#pragma once
#include <array>
#include "Common/CommonTypes.h"

#define NETPLAY_CODE_SIZE 8
typedef std::array<char, NETPLAY_CODE_SIZE> TraversalHostId;
typedef u64 TraversalRequestId;

enum TraversalPacketType
{
  // [*->*]
  TraversalPacketAck = 0,
  // [c->s]
  TraversalPacketPing = 1,
  // [c->s]
  TraversalPacketHelloFromClient = 2,
  // [s->c]
  TraversalPacketHelloFromServer = 3,
  // [c->s] When connecting, first the client asks the central server...
  TraversalPacketConnectPlease = 4,
  // [s->c] ...who asks the game host to send a UDP packet to the
  // client... (an ack implies success)
  TraversalPacketPleaseSendPacket = 5,
  // [s->c] ...which the central server relays back to the client.
  TraversalPacketConnectReady = 6,
  // [s->c] Alternately, the server might not have heard of this host.
  TraversalPacketConnectFailed = 7
};

enum
{
  TraversalProtoVersion = 0
};

enum TraversalConnectFailedReason
{
  TraversalConnectFailedClientDidntRespond = 0,
  TraversalConnectFailedClientFailure,
  TraversalConnectFailedNoSuchClient
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
  u8 type;
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
      TraversalInetAddress yourAddress;  // currently unused
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
      u8 reason;
    } connectFailed;
  };
};
#pragma pack(pop)
