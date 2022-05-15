// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <netinet/in.h>
#endif

#include <SFML/Network.hpp>
#include "Common/CommonTypes.h"
#include "Common/Network.h"

constexpr u16 TCP_FLAG_SIN = 0x200;
constexpr u16 TCP_FLAG_ACK = 0x1000;
constexpr u16 TCP_FLAG_PSH = 0x800;
constexpr u16 TCP_FLAG_FIN = 0x100;
constexpr u16 TCP_FLAG_RST = 0x400;

constexpr u16 IP_PROTOCOL = 0x800;
constexpr u16 ARP_PROTOCOL = 0x806;

constexpr u8 MAX_TCP_BUFFER = 4;

struct TcpBuffer
{
  bool used;
  u64 tick;
  u32 seq_id;
  u16 data_size;
  std::array<u8, 2048> data;
};

struct StackRef
{
  u32 ip;
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
  sf::UdpSocket udp_socket;
  sf::TcpSocket tcp_socket;
  u64 poke_time;
};
