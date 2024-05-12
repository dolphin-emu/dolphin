// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Network.h"

#include <algorithm>
#include <bit>
#include <string_view>
#include <vector>

#ifndef _WIN32
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#else
#include <WinSock2.h>
#endif

#include <fmt/format.h>

#include "Common/BitUtils.h"
#include "Common/CommonFuncs.h"
#include "Common/Random.h"
#include "Common/StringUtil.h"

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
    char c = Common::ToLower(mac_string.at(i));
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

EthernetHeader::EthernetHeader(u16 ether_type) : ethertype(htons(ether_type))
{
}

EthernetHeader::EthernetHeader(const MACAddress& dest, const MACAddress& src, u16 ether_type)
    : destination(dest), source(src), ethertype(htons(ether_type))
{
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
  std::memcpy(source_addr.data(), &from.sin_addr, IPV4_ADDR_LEN);
  std::memcpy(destination_addr.data(), &to.sin_addr, IPV4_ADDR_LEN);

  header_checksum = htons(ComputeNetworkChecksum(this, Size()));
}

u16 IPv4Header::Size() const
{
  return static_cast<u16>(SIZE);
}

u8 IPv4Header::DefinedSize() const
{
  return (version_ihl & 0xf) * 4;
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

TCPHeader::TCPHeader(const sockaddr_in& from, const sockaddr_in& to, u32 seq, u32 ack, u16 flags)
{
  source_port = from.sin_port;
  destination_port = to.sin_port;
  sequence_number = htonl(seq);
  acknowledgement_number = htonl(ack);
  properties = htons(flags);

  window_size = 0x7c;
  checksum = 0;
}

u8 TCPHeader::GetHeaderSize() const
{
  return (ntohs(properties) & 0xf000) >> 10;
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

ARPHeader::ARPHeader() = default;

ARPHeader::ARPHeader(u32 from_ip, const MACAddress& from_mac, u32 to_ip, const MACAddress& to_mac)
{
  hardware_type = htons(BBA_HARDWARE_TYPE);
  protocol_type = IPV4_HEADER_TYPE;
  hardware_size = MAC_ADDRESS_SIZE;
  protocol_size = IPV4_ADDR_LEN;
  opcode = 0x200;
  sender_ip = from_ip;
  target_ip = to_ip;
  targer_address = to_mac;
  sender_address = from_mac;
}

u16 ARPHeader::Size() const
{
  return static_cast<u16>(SIZE);
}

DHCPBody::DHCPBody() = default;

DHCPBody::DHCPBody(u32 transaction, const MACAddress& client_address, u32 new_ip, u32 serv_ip)
{
  transaction_id = transaction;
  message_type = DHCPConst::MESSAGE_REPLY;
  hardware_type = BBA_HARDWARE_TYPE;
  hardware_addr = MAC_ADDRESS_SIZE;
  client_mac = client_address;
  your_ip = new_ip;
  server_ip = serv_ip;
}

DHCPPacket::DHCPPacket() = default;

DHCPPacket::DHCPPacket(const std::vector<u8>& data)
{
  if (data.size() < DHCPBody::SIZE)
    return;
  body = Common::BitCastPtr<DHCPBody>(data.data());
  std::size_t offset = DHCPBody::SIZE;

  while (offset < data.size() - 1)
  {
    const u8 fnc = data[offset];
    if (fnc == 0)
    {
      ++offset;
      continue;
    }
    if (fnc == 255)
      break;
    const u8 len = data[offset + 1];
    const auto opt_begin = data.begin() + offset;
    offset += 2 + len;
    if (offset > data.size())
      break;
    const auto opt_end = data.begin() + offset;
    options.emplace_back(opt_begin, opt_end);
  }
}

void DHCPPacket::AddOption(u8 fnc, const std::vector<u8>& params)
{
  if (params.size() > 255)
    return;
  std::vector<u8> opt = {fnc, u8(params.size())};
  opt.insert(opt.end(), params.begin(), params.end());
  options.emplace_back(std::move(opt));
}

std::vector<u8> DHCPPacket::Build() const
{
  const u8* body_ptr = reinterpret_cast<const u8*>(&body);
  std::vector<u8> result(body_ptr, body_ptr + DHCPBody::SIZE);

  for (auto& opt : options)
  {
    result.insert(result.end(), opt.begin(), opt.end());
  }
  const std::vector<u8> no_option = {255, 0, 0, 0};
  result.insert(result.end(), no_option.begin(), no_option.end());

  return result;
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

// Compute the TCP checksum with its pseudo header
u16 ComputeTCPNetworkChecksum(const IPAddress& from, const IPAddress& to, const void* data,
                              u16 length, u8 protocol)
{
  const u32 source_addr = ntohl(std::bit_cast<u32>(from));
  const u32 destination_addr = ntohl(std::bit_cast<u32>(to));
  const u32 initial_value = (source_addr >> 16) + (source_addr & 0xFFFF) +
                            (destination_addr >> 16) + (destination_addr & 0xFFFF) + protocol +
                            length;
  const u32 tcp_checksum = ComputeNetworkChecksum(data, length, initial_value);
  return htons(static_cast<u16>(tcp_checksum));
}

template <typename Container, typename T>
static inline void InsertObj(Container* container, const T& obj)
{
  static_assert(std::is_trivially_copyable_v<T>);
  const u8* const ptr = reinterpret_cast<const u8*>(&obj);
  container->insert(container->end(), ptr, ptr + sizeof(obj));
}

ARPPacket::ARPPacket() = default;

u16 ARPPacket::Size() const
{
  return static_cast<u16>(SIZE);
}

ARPPacket::ARPPacket(const MACAddress& destination, const MACAddress& source)
{
  eth_header.destination = destination;
  eth_header.source = source;
  eth_header.ethertype = htons(ARP_ETHERTYPE);
}

std::vector<u8> ARPPacket::Build() const
{
  std::vector<u8> result;
  result.reserve(EthernetHeader::SIZE + ARPHeader::SIZE);
  InsertObj(&result, eth_header);
  InsertObj(&result, arp_header);
  return result;
}

TCPPacket::TCPPacket() = default;

TCPPacket::TCPPacket(const MACAddress& destination, const MACAddress& source,
                     const sockaddr_in& from, const sockaddr_in& to, u32 seq, u32 ack, u16 flags)
    : eth_header(destination, source, IPV4_ETHERTYPE),
      ip_header(Common::TCPHeader::SIZE, IPPROTO_TCP, from, to),
      tcp_header(from, to, seq, ack, flags)
{
}

std::vector<u8> TCPPacket::Build() const
{
  std::vector<u8> result;
  result.reserve(Size());  // Useful not to invalidate .data() pointers

  // Copy data
  InsertObj(&result, eth_header);
  u8* const ip_ptr = result.data() + result.size();
  InsertObj(&result, ip_header);
  result.insert(result.end(), ipv4_options.begin(), ipv4_options.end());
  u8* const tcp_ptr = result.data() + result.size();
  InsertObj(&result, tcp_header);
  result.insert(result.end(), tcp_options.begin(), tcp_options.end());
  result.insert(result.end(), data.begin(), data.end());

  // Adjust size and checksum fields
  const u16 tcp_length = static_cast<u16>(TCPHeader::SIZE + tcp_options.size() + data.size());
  const u16 tcp_properties =
      (ntohs(tcp_header.properties) & 0xfff) |
      (static_cast<u16>((tcp_options.size() + TCPHeader::SIZE) & 0x3c) << 10);
  Common::BitCastPtr<u16>(tcp_ptr + offsetof(TCPHeader, properties)) = htons(tcp_properties);

  const u16 ip_header_size = static_cast<u16>(IPv4Header::SIZE + ipv4_options.size());
  const u16 ip_total_len = ip_header_size + tcp_length;
  Common::BitCastPtr<u16>(ip_ptr + offsetof(IPv4Header, total_len)) = htons(ip_total_len);

  auto ip_checksum_bitcast_ptr =
      Common::BitCastPtr<u16>(ip_ptr + offsetof(IPv4Header, header_checksum));
  ip_checksum_bitcast_ptr = u16(0);
  ip_checksum_bitcast_ptr = htons(Common::ComputeNetworkChecksum(ip_ptr, ip_header_size));

  auto checksum_bitcast_ptr = Common::BitCastPtr<u16>(tcp_ptr + offsetof(TCPHeader, checksum));
  checksum_bitcast_ptr = u16(0);
  checksum_bitcast_ptr = ComputeTCPNetworkChecksum(
      ip_header.source_addr, ip_header.destination_addr, tcp_ptr, tcp_length, IPPROTO_TCP);

  return result;
}

u16 TCPPacket::Size() const
{
  return static_cast<u16>(MIN_SIZE + data.size() + ipv4_options.size() + tcp_options.size());
}

UDPPacket::UDPPacket() = default;

UDPPacket::UDPPacket(const MACAddress& destination, const MACAddress& source,
                     const sockaddr_in& from, const sockaddr_in& to, const std::vector<u8>& payload)
    : eth_header(destination, source, IPV4_ETHERTYPE),
      ip_header(static_cast<u16>(payload.size() + Common::UDPHeader::SIZE), IPPROTO_UDP, from, to),
      udp_header(from, to, static_cast<u16>(payload.size())), data(payload)
{
}

std::vector<u8> UDPPacket::Build() const
{
  std::vector<u8> result;
  result.reserve(Size());  // Useful not to invalidate .data() pointers

  // Copy data
  InsertObj(&result, eth_header);
  u8* const ip_ptr = result.data() + result.size();
  InsertObj(&result, ip_header);
  result.insert(result.end(), ipv4_options.begin(), ipv4_options.end());
  u8* const udp_ptr = result.data() + result.size();
  InsertObj(&result, udp_header);
  result.insert(result.end(), data.begin(), data.end());

  // Adjust size and checksum fields
  const u16 udp_length = static_cast<u16>(UDPHeader::SIZE + data.size());
  Common::BitCastPtr<u16>(udp_ptr + offsetof(UDPHeader, length)) = htons(udp_length);

  const u16 ip_header_size = static_cast<u16>(IPv4Header::SIZE + ipv4_options.size());
  const u16 ip_total_len = ip_header_size + udp_length;
  Common::BitCastPtr<u16>(ip_ptr + offsetof(IPv4Header, total_len)) = htons(ip_total_len);

  auto ip_checksum_bitcast_ptr =
      Common::BitCastPtr<u16>(ip_ptr + offsetof(IPv4Header, header_checksum));
  ip_checksum_bitcast_ptr = u16(0);
  ip_checksum_bitcast_ptr = htons(Common::ComputeNetworkChecksum(ip_ptr, ip_header_size));

  auto checksum_bitcast_ptr = Common::BitCastPtr<u16>(udp_ptr + offsetof(UDPHeader, checksum));
  checksum_bitcast_ptr = u16(0);
  checksum_bitcast_ptr = ComputeTCPNetworkChecksum(
      ip_header.source_addr, ip_header.destination_addr, udp_ptr, udp_length, IPPROTO_UDP);

  return result;
}

u16 UDPPacket::Size() const
{
  return static_cast<u16>(MIN_SIZE + data.size() + ipv4_options.size());
}

PacketView::PacketView(const u8* ptr, std::size_t size) : m_ptr(ptr), m_size(size)
{
}

std::optional<u16> PacketView::GetEtherType() const
{
  if (m_size < EthernetHeader::SIZE)
    return std::nullopt;
  const std::size_t offset = offsetof(EthernetHeader, ethertype);
  return ntohs(Common::BitCastPtr<u16>(m_ptr + offset));
}

std::optional<ARPPacket> PacketView::GetARPPacket() const
{
  if (m_size < ARPPacket::SIZE)
    return std::nullopt;
  return Common::BitCastPtr<ARPPacket>(m_ptr);
}

std::optional<u8> PacketView::GetIPProto() const
{
  if (m_size < EthernetHeader::SIZE + IPv4Header::SIZE)
    return std::nullopt;
  return m_ptr[EthernetHeader::SIZE + offsetof(IPv4Header, protocol)];
}

std::optional<TCPPacket> PacketView::GetTCPPacket() const
{
  if (m_size < TCPPacket::MIN_SIZE)
    return std::nullopt;
  TCPPacket result;
  result.eth_header = Common::BitCastPtr<EthernetHeader>(m_ptr);
  result.ip_header = Common::BitCastPtr<IPv4Header>(m_ptr + EthernetHeader::SIZE);
  const u16 offset = result.ip_header.DefinedSize() + EthernetHeader::SIZE;
  if (m_size < offset + TCPHeader::SIZE)
    return std::nullopt;
  result.ipv4_options =
      std::vector<u8>(m_ptr + EthernetHeader::SIZE + IPv4Header::SIZE, m_ptr + offset);
  result.tcp_header = Common::BitCastPtr<TCPHeader>(m_ptr + offset);
  const u16 data_offset = result.tcp_header.GetHeaderSize() + offset;

  const u16 total_len = ntohs(result.ip_header.total_len);
  const std::size_t end = EthernetHeader::SIZE + total_len;

  if (m_size < end || end < data_offset)
    return std::nullopt;

  result.tcp_options = std::vector<u8>(m_ptr + offset + TCPHeader::SIZE, m_ptr + data_offset);
  result.data = std::vector<u8>(m_ptr + data_offset, m_ptr + end);

  return result;
}

std::optional<UDPPacket> PacketView::GetUDPPacket() const
{
  if (m_size < UDPPacket::MIN_SIZE)
    return std::nullopt;
  UDPPacket result;
  result.eth_header = Common::BitCastPtr<EthernetHeader>(m_ptr);
  result.ip_header = Common::BitCastPtr<IPv4Header>(m_ptr + EthernetHeader::SIZE);
  const u16 offset = result.ip_header.DefinedSize() + EthernetHeader::SIZE;
  if (m_size < offset + UDPHeader::SIZE)
    return std::nullopt;
  result.ipv4_options =
      std::vector<u8>(m_ptr + EthernetHeader::SIZE + IPv4Header::SIZE, m_ptr + offset);
  result.udp_header = Common::BitCastPtr<UDPHeader>(m_ptr + offset);
  const u16 data_offset = UDPHeader::SIZE + offset;

  const u16 total_len = ntohs(result.udp_header.length);
  const std::size_t end = offset + total_len;

  if (m_size < end || end < data_offset)
    return std::nullopt;

  result.data = std::vector<u8>(m_ptr + data_offset, m_ptr + end);

  return result;
}

NetworkErrorState SaveNetworkErrorState()
{
  return {
      errno,
#ifdef _WIN32
      WSAGetLastError(),
#endif
  };
}

void RestoreNetworkErrorState(const NetworkErrorState& state)
{
  errno = state.error;
#ifdef _WIN32
  WSASetLastError(state.wsa_error);
#endif
}

const char* DecodeNetworkError(s32 error_code)
{
  thread_local char buffer[1024];

#ifdef _WIN32
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                     FORMAT_MESSAGE_MAX_WIDTH_MASK,
                 nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer,
                 sizeof(buffer), nullptr);
  return buffer;
#else
  return Common::StrErrorWrapper(error_code, buffer, sizeof(buffer));
#endif
}

const char* StrNetworkError()
{
#ifdef _WIN32
  const s32 error_code = WSAGetLastError();
#else
  const s32 error_code = errno;
#endif
  return DecodeNetworkError(error_code);
}
}  // namespace Common
