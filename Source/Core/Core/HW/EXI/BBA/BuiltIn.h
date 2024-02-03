// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef _WIN32
#include <WinSock2.h>
using socklen_t = int;
#else
#include <netinet/in.h>
#endif

#include <SFML/Network.hpp>
#include "Common/CommonTypes.h"
#include "Common/Network.h"

constexpr u16 TCP_FLAG_SIN = 0x2;
constexpr u16 TCP_FLAG_ACK = 0x10;
constexpr u16 TCP_FLAG_PSH = 0x8;
constexpr u16 TCP_FLAG_FIN = 0x1;
constexpr u16 TCP_FLAG_RST = 0x4;

constexpr u8 MAX_TCP_BUFFER = 4;
constexpr u16 MAX_UDP_LENGTH = 1500;
constexpr u16 MAX_TCP_LENGTH = 440;

struct TcpBuffer
{
  bool used;
  u64 tick;
  u32 seq_id;
  std::vector<u8> data;
};

struct StackRef;

// Socket helper classes to ensure network interface consistency.
//
// If the socket isn't bound, the system will pick the interface to use automatically.
// This might result in the source IP address changing when talking to the same peer
// and multiple interfaces/IP addresses can reach the socket's peer.
class BbaTcpSocket : public sf::TcpSocket
{
public:
  BbaTcpSocket();

  sf::Socket::Status Connect(const sf::IpAddress& dest, u16 port, u32 net_ip);
  sf::Socket::Status GetPeerName(sockaddr_in* addr) const;
  sf::Socket::Status GetSockName(sockaddr_in* addr) const;

  enum class ConnectingState
  {
    None,
    Connecting,
    Connected,
    Error
  };

  ConnectingState Connected(StackRef* ref);

private:
  ConnectingState m_connecting_state = ConnectingState::None;
};

class BbaUdpSocket : public sf::UdpSocket
{
public:
  BbaUdpSocket();

  sf::Socket::Status Bind(u16 port, u32 net_ip);
};

struct StackRef
{
  u32 ip = 0;
  u16 local;
  u16 remote;
  u16 type;
  sf::IpAddress target;
  u32 seq_num;
  u32 ack_num;
  u32 ack_base;
  u16 window_size;
  u64 delay;
  std::array<TcpBuffer, MAX_TCP_BUFFER> tcp_buffers;
  bool ready;
  sockaddr_in from;
  sockaddr_in to;
  Common::MACAddress bba_mac{};
  Common::MACAddress my_mac{};
  BbaUdpSocket udp_socket;
  BbaTcpSocket tcp_socket;
  u64 poke_time;
};

// Max 10 at same time, I think most gc game had a
// limit of 8 in the GC framework
using StackRefs = std::array<StackRef, 10>;

class NetworkRef
{
public:
  StackRefs& data() { return m_stacks; }
  const StackRefs& data() const { return m_stacks; }
  auto begin() { return m_stacks.begin(); }
  auto begin() const { return m_stacks.cbegin(); }
  auto end() { return m_stacks.end(); }
  auto end() const { return m_stacks.cend(); }

  StackRef* GetAvailableSlot(u16 port);
  StackRef* GetTCPSlot(u16 src_port, u16 dst_port, u32 ip);

  void Clear();

private:
  StackRefs m_stacks;
};
