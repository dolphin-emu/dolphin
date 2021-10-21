// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Network.h"

#include <algorithm>
#include <cctype>
#include <string_view>

#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#else
#include <WinSock2.h>
#endif

#include <fmt/format.h>

#include "Common/Random.h"

namespace Common
{
MACAddress GenerateMacAddress(const MACConsumer type)
{
  constexpr std::array<u8, 3> oui_bba{{0x00, 0x09, 0xbf}};
  constexpr std::array<u8, 3> oui_ios{{0x00, 0x17, 0xab}};

  MACAddress mac{};

  switch (type)
  {
  case MACConsumer::BBA:
    std::copy(oui_bba.begin(), oui_bba.end(), mac.begin());
    break;
  case MACConsumer::IOS:
    std::copy(oui_ios.begin(), oui_ios.end(), mac.begin());
    break;
  }

  // Generate the 24-bit NIC-specific portion of the MAC address.
  Random::Generate(&mac[3], 3);
  return mac;
}

std::string MacAddressToString(const MACAddress& mac)
{
  return fmt::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", mac[0], mac[1], mac[2], mac[3],
                     mac[4], mac[5]);
}

std::optional<MACAddress> StringToMacAddress(std::string_view mac_string)
{
  if (mac_string.empty())
    return std::nullopt;

  int x = 0;
  MACAddress mac{};

  for (size_t i = 0; i < mac_string.size() && x < (MAC_ADDRESS_SIZE * 2); ++i)
  {
    char c = tolower(mac_string.at(i));
    if (c >= '0' && c <= '9')
    {
      mac[x / 2] |= (c - '0') << ((x & 1) ? 0 : 4);
      ++x;
    }
    else if (c >= 'a' && c <= 'f')
    {
      mac[x / 2] |= (c - 'a' + 10) << ((x & 1) ? 0 : 4);
      ++x;
    }
  }

  // A valid 48-bit MAC address consists of 6 octets, where each
  // nibble is a character in the MAC address, making 12 characters
  // in total.
  if (x / 2 != MAC_ADDRESS_SIZE)
    return std::nullopt;

  return std::make_optional(mac);
}

EthernetHeader::EthernetHeader() = default;

EthernetHeader::EthernetHeader(u16 ether_type)
{
  ethertype = htons(ether_type);
}

u16 EthernetHeader::Size() const
{
  return static_cast<u16>(SIZE);
}

IPv4Header::IPv4Header() = default;

IPv4Header::IPv4Header(u16 data_size, u8 ip_proto, const sockaddr_in& from, const sockaddr_in& to)
{
  version_ihl = 0x45;
  total_len = htons(Size() + data_size);
  flags_fragment_offset = htons(0x4000);
  ttl = 0x40;
  protocol = ip_proto;
  std::memcpy(&source_addr, &from.sin_addr, IPV4_ADDR_LEN);
  std::memcpy(&destination_addr, &to.sin_addr, IPV4_ADDR_LEN);

  header_checksum = htons(ComputeNetworkChecksum(this, Size()));
}

u16 IPv4Header::Size() const
{
  return static_cast<u16>(SIZE);
}

TCPHeader::TCPHeader() = default;

TCPHeader::TCPHeader(const sockaddr_in& from, const sockaddr_in& to, u32 seq, const u8* data,
                     u16 length)
{
  std::memcpy(&source_port, &from.sin_port, 2);
  std::memcpy(&destination_port, &to.sin_port, 2);
  sequence_number = htonl(seq);

  // TODO: Write flags
  // Write data offset
  std::memset(&properties, 0x50, 1);

  window_size = 0xFFFF;

  // Compute the TCP checksum with its pseudo header
  const u32 source_addr = ntohl(from.sin_addr.s_addr);
  const u32 destination_addr = ntohl(to.sin_addr.s_addr);
  const u32 initial_value = (source_addr >> 16) + (source_addr & 0xFFFF) +
                            (destination_addr >> 16) + (destination_addr & 0xFFFF) + IPProto() +
                            Size() + length;
  u32 tcp_checksum = ComputeNetworkChecksum(this, Size(), initial_value);
  tcp_checksum += ComputeNetworkChecksum(data, length);
  while (tcp_checksum > 0xFFFF)
    tcp_checksum = (tcp_checksum >> 16) + (tcp_checksum & 0xFFFF);
  checksum = htons(static_cast<u16>(tcp_checksum));
}

u16 TCPHeader::Size() const
{
  return static_cast<u16>(SIZE);
}

u8 TCPHeader::IPProto() const
{
  return static_cast<u8>(IPPROTO_TCP);
}

UDPHeader::UDPHeader() = default;

UDPHeader::UDPHeader(const sockaddr_in& from, const sockaddr_in& to, u16 data_length)
{
  std::memcpy(&source_port, &from.sin_port, 2);
  std::memcpy(&destination_port, &to.sin_port, 2);
  length = htons(Size() + data_length);
}

u16 UDPHeader::Size() const
{
  return static_cast<u16>(SIZE);
}

u8 UDPHeader::IPProto() const
{
  return static_cast<u8>(IPPROTO_UDP);
}

// Compute the network checksum with a 32-bit accumulator using the
// "Normal" order, see RFC 1071 for more details.
u16 ComputeNetworkChecksum(const void* data, u16 length, u32 initial_value)
{
  u32 checksum = initial_value;
  std::size_t index = 0;
  const std::string_view data_view{reinterpret_cast<const char*>(data), length};
  for (u8 b : data_view)
  {
    const bool is_hi = index++ % 2 == 0;
    checksum += is_hi ? b << 8 : b;
  }
  while (checksum > 0xFFFF)
    checksum = (checksum >> 16) + (checksum & 0xFFFF);
  return ~static_cast<u16>(checksum);
}
}  // namespace Common
