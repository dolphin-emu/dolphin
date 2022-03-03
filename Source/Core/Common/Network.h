// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "Common/CommonTypes.h"

struct sockaddr_in;

namespace Common
{
enum class MACConsumer
{
  BBA,
  IOS
};

enum
{
  MAC_ADDRESS_SIZE = 6
};

using MACAddress = std::array<u8, MAC_ADDRESS_SIZE>;
constexpr std::size_t IPV4_ADDR_LEN = 4;

struct EthernetHeader
{
  EthernetHeader();
  explicit EthernetHeader(u16 ether_type);
  u16 Size() const;

  static constexpr std::size_t SIZE = 14;

  MACAddress destination = {};
  MACAddress source = {};
  u16 ethertype = 0;
};
static_assert(sizeof(EthernetHeader) == EthernetHeader::SIZE);

struct IPv4Header
{
  IPv4Header();
  IPv4Header(u16 data_size, u8 ip_proto, const sockaddr_in& from, const sockaddr_in& to);
  u16 Size() const;

  static constexpr std::size_t SIZE = 20;

  u8 version_ihl = 0;
  u8 dscp_esn = 0;
  u16 total_len = 0;
  u16 identification = 0;
  u16 flags_fragment_offset = 0;
  u8 ttl = 0;
  u8 protocol = 0;
  u16 header_checksum = 0;
  u8 source_addr[IPV4_ADDR_LEN]{};
  u8 destination_addr[IPV4_ADDR_LEN]{};
};
static_assert(sizeof(IPv4Header) == IPv4Header::SIZE);

struct TCPHeader
{
  TCPHeader();
  TCPHeader(const sockaddr_in& from, const sockaddr_in& to, u32 seq, const u8* data, u16 length);
  u16 Size() const;
  u8 IPProto() const;

  static constexpr std::size_t SIZE = 20;

  u16 source_port = 0;
  u16 destination_port = 0;
  u32 sequence_number = 0;
  u32 acknowledgement_number = 0;
  u16 properties = 0;
  u16 window_size = 0;
  u16 checksum = 0;
  u16 urgent_pointer = 0;
};
static_assert(sizeof(TCPHeader) == TCPHeader::SIZE);

struct UDPHeader
{
  UDPHeader();
  UDPHeader(const sockaddr_in& from, const sockaddr_in& to, u16 data_length);
  u16 Size() const;
  u8 IPProto() const;

  static constexpr std::size_t SIZE = 8;

  u16 source_port = 0;
  u16 destination_port = 0;
  u16 length = 0;
  u16 checksum = 0;
};
static_assert(sizeof(UDPHeader) == UDPHeader::SIZE);

MACAddress GenerateMacAddress(MACConsumer type);
std::string MacAddressToString(const MACAddress& mac);
std::optional<MACAddress> StringToMacAddress(std::string_view mac_string);
u16 ComputeNetworkChecksum(const void* data, u16 length, u32 initial_value = 0);
}  // namespace Common
