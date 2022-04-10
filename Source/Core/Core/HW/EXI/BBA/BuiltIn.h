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

#define TCP_FLAG_SIN 0x200
#define TCP_FLAG_ACK 0x1000
#define TCP_FLAG_PSH 0x800
#define TCP_FLAG_FIN 0x100
#define TCP_FLAG_RST 0x400

#define IP_PROTOCOL 0x800
#define ARP_PROTOCOL 0x806

constexpr u8 MAX_TCP_BUFFER = 4;


struct TcpBuffer
{
  bool used;
  unsigned long long tick;
  u32 seq_id;
  u16 data_size;
  char data[2048];
};
struct StackRef
{
  unsigned int ip;
  unsigned short local;
  unsigned short remote;
  unsigned short type;
  sf::IpAddress target;
  u32 seq_num;
  u32 ack_num;
  u32 ack_base;
  u16 window_size;
  u16 delay;
  TcpBuffer TcpBuffers[MAX_TCP_BUFFER];
  bool ready;
  sockaddr_in from;
  sockaddr_in to;
  Common::MACAddress bba_mac{};
  Common::MACAddress my_mac{};
  sf::UdpSocket udp_socket;
  sf::TcpSocket tcp_socket;
};
