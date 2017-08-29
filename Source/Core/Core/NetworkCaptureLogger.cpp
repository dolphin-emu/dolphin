// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/NetworkCaptureLogger.h"

#ifdef _WIN32
#include <WinSock2.h>
using socklen_t = int;
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <sstream>

#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Network.h"
#include "Common/PcapFile.h"
#include "Core/ConfigManager.h"

namespace Core
{
NetworkCaptureLogger::~NetworkCaptureLogger() = default;

void DummyNetworkCaptureLogger::LogSSLRead(const void* data, std::size_t length, s32 socket)
{
}

void DummyNetworkCaptureLogger::LogSSLWrite(const void* data, std::size_t length, s32 socket)
{
}

void DummyNetworkCaptureLogger::LogTCPRead(const void* data, std::size_t length, s32 socket)
{
}

void DummyNetworkCaptureLogger::LogTCPWrite(const void* data, std::size_t length, s32 socket)
{
}

void DummyNetworkCaptureLogger::LogUDPRead(const void* data, std::size_t length, s32 socket)
{
}

void DummyNetworkCaptureLogger::LogUDPWrite(const void* data, std::size_t length, s32 socket)
{
}

void DummyNetworkCaptureLogger::LogRead(const void* data, std::size_t length, s32 socket)
{
}

void DummyNetworkCaptureLogger::LogWrite(const void* data, std::size_t length, s32 socket)
{
}

BinarySSLCaptureLogger::BinarySSLCaptureLogger()
{
  if (SConfig::GetInstance().m_SSLDumpRead)
  {
    m_file_read.Open(
        File::GetUserPath(D_DUMPSSL_IDX) + SConfig::GetInstance().GetGameID() + "_read.bin", "ab");
  }
  if (SConfig::GetInstance().m_SSLDumpWrite)
  {
    m_file_write.Open(
        File::GetUserPath(D_DUMPSSL_IDX) + SConfig::GetInstance().GetGameID() + "_write.bin", "ab");
  }
}

void BinarySSLCaptureLogger::LogSSLRead(const void* data, std::size_t length, s32 socket)
{
  m_file_read.WriteBytes(data, length);
}

void BinarySSLCaptureLogger::LogSSLWrite(const void* data, std::size_t length, s32 socket)
{
  m_file_write.WriteBytes(data, length);
}

PCAPSSLCaptureLogger::PCAPSSLCaptureLogger()
{
  std::time_t t = std::time(nullptr);
  std::ostringstream ss;
  ss << std::put_time(std::localtime(&t), " %Y-%m-%d %Hh%Mm%Ss.pcap");

  const std::string filename =
      File::GetUserPath(D_DUMPSSL_IDX) + SConfig::GetInstance().GetGameID() + ss.str();
  m_file = std::make_unique<PCAP>(new File::IOFile(filename, "wb"), PCAP::LinkType::Ethernet);
}

PCAPSSLCaptureLogger::~PCAPSSLCaptureLogger() = default;

PCAPSSLCaptureLogger::ErrorState PCAPSSLCaptureLogger::SaveState() const
{
  return {
      errno,
#ifdef _WIN32
      WSAGetLastError(),
#endif
  };
}

void PCAPSSLCaptureLogger::RestoreState(const PCAPSSLCaptureLogger::ErrorState& state) const
{
  errno = state.error;
#ifdef _WIN32
  WSASetLastError(state.wsa_error);
#endif
}

void PCAPSSLCaptureLogger::Log(LogType log_type, bool is_sender, const void* data,
                               std::size_t length, s32 socket)
{
  const auto state = SaveState();
  if (log_type == LogType::UDP)
    LogUDP(is_sender, data, length, socket);
  else
    LogTCP(is_sender, data, length, socket);
  RestoreState(state);
}

void PCAPSSLCaptureLogger::LogSSLRead(const void* data, std::size_t length, s32 socket)
{
  Log(LogType::SSL, false, data, length, socket);
}

void PCAPSSLCaptureLogger::LogSSLWrite(const void* data, std::size_t length, s32 socket)
{
  Log(LogType::SSL, true, data, length, socket);
}

void PCAPSSLCaptureLogger::LogTCPRead(const void* data, std::size_t length, s32 socket)
{
  Log(LogType::TCP, false, data, length, socket);
}

void PCAPSSLCaptureLogger::LogTCPWrite(const void* data, std::size_t length, s32 socket)
{
  Log(LogType::TCP, true, data, length, socket);
}

void PCAPSSLCaptureLogger::LogUDPRead(const void* data, std::size_t length, s32 socket)
{
  Log(LogType::UDP, false, data, length, socket);
}

void PCAPSSLCaptureLogger::LogUDPWrite(const void* data, std::size_t length, s32 socket)
{
  Log(LogType::UDP, true, data, length, socket);
}

void PCAPSSLCaptureLogger::LogRead(const void* data, std::size_t length, s32 socket)
{
  int socket_type;
  socklen_t option_length = sizeof(int);

  const auto state = SaveState();
  if (getsockopt(socket, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&socket_type),
                 &option_length) == 0)
  {
    if (socket_type == SOCK_STREAM)
      LogTCPRead(data, length, socket);
    else if (socket_type == SOCK_DGRAM)
      LogUDPRead(data, length, socket);
  }
  RestoreState(state);
}

void PCAPSSLCaptureLogger::LogWrite(const void* data, std::size_t length, s32 socket)
{
  int socket_type;
  socklen_t option_length = sizeof(int);

  const auto state = SaveState();
  if (getsockopt(socket, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&socket_type),
                 &option_length) == 0)
  {
    if (socket_type == SOCK_STREAM)
      LogTCPWrite(data, length, socket);
    else if (socket_type == SOCK_DGRAM)
      LogUDPWrite(data, length, socket);
  }
  RestoreState(state);
}

PCAPSSLCaptureLogger::EthernetHeader PCAPSSLCaptureLogger::GetEthernetHeader(bool is_sender) const
{
  EthernetHeader ethernet_header = {};
  std::array<u8, Common::MAC_ADDRESS_SIZE> mac = {};

  // Get Dolphin MAC address
  const std::string mac_address = SConfig::GetInstance().m_WirelessMac;
  if (!Common::StringToMacAddress(mac_address, mac.data()))
    mac.fill(0);

  // Write Dolphin MAC address, the other one is dummy
  if (is_sender)
  {
    // Source MAC address
    std::copy(std::begin(mac), std::end(mac), ethernet_header.begin() + 6);
  }
  else
  {
    // Destination MAC address
    std::copy(std::begin(mac), std::end(mac), ethernet_header.begin());
  }

  // Write the proctol type (IP = 08 00)
  ethernet_header[12] = 8;
  return ethernet_header;
}

PCAPSSLCaptureLogger::IPHeader PCAPSSLCaptureLogger::GetIPHeader(bool is_sender,
                                                                 std::size_t data_size, u8 protocol,
                                                                 s32 socket) const
{
  IPHeader ip_header = {};
  const std::size_t total_length = IP_HEADER_SIZE + data_size;

  // Write IP version and header length
  ip_header[0] = 0x45;
  // Write total length
  ip_header[2] = static_cast<u8>(total_length >> 8 & 0xFF);
  ip_header[3] = static_cast<u8>(total_length & 0xFF);
  // Write flags + fragment offset
  ip_header[6] = 0x40;
  // Write TTL
  ip_header[8] = 0x40;
  // Write protocol
  ip_header[9] = protocol;

  // Get socket IP address
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);
  getpeername(socket, reinterpret_cast<struct sockaddr*>(&addr), &addr_len);

  if (is_sender)
  {
    // Write Source IP address: 127.0.0.1
    ip_header[12] = 0x7F;
    ip_header[15] = 0x01;
    // Write Destination IP address
    std::memcpy(&ip_header[16], &addr.sin_addr, 4);
  }
  else
  {
    // Write Source IP address
    std::memcpy(&ip_header[12], &addr.sin_addr, 4);
    // Write Destination IP address: 127.0.0.1
    ip_header[16] = 0x7F;
    ip_header[19] = 0x01;
  }

  // Compute checksum
  std::size_t index = 0;
  u32 checksum = 0;
  u16 word = 0;
  for (u8 b : ip_header)
  {
    if (index % 2 == 0)
      word = b;
    else
      checksum += (word << 8) + b;
    index += 1;
  }
  checksum = (checksum + (checksum >> 16) & 0xFFFF) ^ 0xFFFF;

  // Write checksum
  ip_header[10] = static_cast<u8>(checksum >> 8 & 0xFF);
  ip_header[11] = static_cast<u8>(checksum & 0xFF);

  return ip_header;
}

void PCAPSSLCaptureLogger::LogTCP(bool is_sender, const void* data, std::size_t length, s32 socket)
{
  TCPHeader tcp_header = {};

  // Get socket port
  struct sockaddr_in sock_addr;
  struct sockaddr_in peer_addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);
  getsockname(socket, reinterpret_cast<struct sockaddr*>(&sock_addr), &addr_len);
  getpeername(socket, reinterpret_cast<struct sockaddr*>(&peer_addr), &addr_len);

  // Write port
  if (is_sender)
  {
    std::memcpy(&tcp_header[0], &sock_addr.sin_port, 2);
    std::memcpy(&tcp_header[2], &peer_addr.sin_port, 2);

    // Write sequence number
    tcp_header[4] = static_cast<u8>(m_write_sequence_number >> 24 & 0xFF);
    tcp_header[5] = static_cast<u8>(m_write_sequence_number >> 16 & 0xFF);
    tcp_header[6] = static_cast<u8>(m_write_sequence_number >> 8 & 0xFF);
    tcp_header[7] = static_cast<u8>(m_write_sequence_number & 0xFF);
    m_write_sequence_number += static_cast<u32>(length);
  }
  else
  {
    std::memcpy(&tcp_header[0], &peer_addr.sin_port, 2);
    std::memcpy(&tcp_header[2], &sock_addr.sin_port, 2);

    // Write sequence number
    tcp_header[4] = static_cast<u8>(m_read_sequence_number >> 24 & 0xFF);
    tcp_header[5] = static_cast<u8>(m_read_sequence_number >> 16 & 0xFF);
    tcp_header[6] = static_cast<u8>(m_read_sequence_number >> 8 & 0xFF);
    tcp_header[7] = static_cast<u8>(m_read_sequence_number & 0xFF);
    m_read_sequence_number += static_cast<u32>(length);
  }

  // Write data offset
  tcp_header[12] = 0x50;

  // Write flags
  // TODO

  // Write window size
  tcp_header[14] = 0xFF;
  tcp_header[15] = 0xFF;

  // Add the packet
  const EthernetHeader ethernet_header = GetEthernetHeader(is_sender);
  const IPHeader ip_header = GetIPHeader(is_sender, TCP_HEADER_SIZE + length, 0x06, socket);
  std::vector<u8> packet;
  packet.insert(packet.end(), ethernet_header.begin(), ethernet_header.end());
  packet.insert(packet.end(), ip_header.begin(), ip_header.end());
  packet.insert(packet.end(), tcp_header.begin(), tcp_header.end());
  packet.insert(packet.end(), static_cast<const char*>(data),
                static_cast<const char*>(data) + length);
  m_file->AddPacket(packet.data(), packet.size());
}

void PCAPSSLCaptureLogger::LogUDP(bool is_sender, const void* data, std::size_t length, s32 socket)
{
  UDPHeader udp_header = {};

  // Get socket port
  struct sockaddr_in sock_addr;
  struct sockaddr_in peer_addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);
  getsockname(socket, reinterpret_cast<struct sockaddr*>(&sock_addr), &addr_len);
  getpeername(socket, reinterpret_cast<struct sockaddr*>(&peer_addr), &addr_len);

  // Write port
  if (is_sender)
  {
    std::memcpy(&udp_header[0], &sock_addr.sin_port, 2);
    std::memcpy(&udp_header[2], &peer_addr.sin_port, 2);
  }
  else
  {
    std::memcpy(&udp_header[0], &peer_addr.sin_port, 2);
    std::memcpy(&udp_header[2], &sock_addr.sin_port, 2);
  }
  const u16 udp_length = static_cast<u16>(UDP_HEADER_SIZE + length);
  udp_header[4] = static_cast<u8>(udp_length >> 8 & 0xFF);
  udp_header[5] = static_cast<u8>(udp_length & 0xFF);

  // Add the packet
  const EthernetHeader ethernet_header = GetEthernetHeader(is_sender);
  const IPHeader ip_header = GetIPHeader(is_sender, UDP_HEADER_SIZE + length, 0x11, socket);
  std::vector<u8> packet;
  packet.insert(packet.end(), ethernet_header.begin(), ethernet_header.end());
  packet.insert(packet.end(), ip_header.begin(), ip_header.end());
  packet.insert(packet.end(), udp_header.begin(), udp_header.end());
  packet.insert(packet.end(), static_cast<const char*>(data),
                static_cast<const char*>(data) + length);
  m_file->AddPacket(packet.data(), packet.size());
}
}  // namespace Core
