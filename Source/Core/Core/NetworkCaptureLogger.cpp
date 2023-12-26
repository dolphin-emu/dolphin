// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/NetworkCaptureLogger.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <iterator>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Network.h"
#include "Common/PcapFile.h"
#include "Common/ScopeGuard.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"

namespace Core
{
NetworkCaptureLogger::NetworkCaptureLogger() = default;
NetworkCaptureLogger::~NetworkCaptureLogger() = default;

void DummyNetworkCaptureLogger::OnNewSocket(s32 socket)
{
}

void DummyNetworkCaptureLogger::LogSSLRead(const void* data, std::size_t length, s32 socket)
{
}

void DummyNetworkCaptureLogger::LogSSLWrite(const void* data, std::size_t length, s32 socket)
{
}

void DummyNetworkCaptureLogger::LogRead(const void* data, std::size_t length, s32 socket,
                                        sockaddr* from)
{
}

void DummyNetworkCaptureLogger::LogWrite(const void* data, std::size_t length, s32 socket,
                                         sockaddr* to)
{
}

void DummyNetworkCaptureLogger::LogBBA(const void* data, std::size_t length)
{
}

NetworkCaptureType DummyNetworkCaptureLogger::GetCaptureType() const
{
  return NetworkCaptureType::None;
}

void BinarySSLCaptureLogger::LogSSLRead(const void* data, std::size_t length, s32 socket)
{
  if (!Config::Get(Config::MAIN_NETWORK_SSL_DUMP_READ))
    return;
  const std::string filename =
      File::GetUserPath(D_DUMPSSL_IDX) + SConfig::GetInstance().GetGameID() + "_read.bin";
  File::IOFile(filename, "ab").WriteBytes(data, length);
}

void BinarySSLCaptureLogger::LogSSLWrite(const void* data, std::size_t length, s32 socket)
{
  if (!Config::Get(Config::MAIN_NETWORK_SSL_DUMP_WRITE))
    return;
  const std::string filename =
      File::GetUserPath(D_DUMPSSL_IDX) + SConfig::GetInstance().GetGameID() + "_write.bin";
  File::IOFile(filename, "ab").WriteBytes(data, length);
}

NetworkCaptureType BinarySSLCaptureLogger::GetCaptureType() const
{
  return NetworkCaptureType::Raw;
}

PCAPSSLCaptureLogger::PCAPSSLCaptureLogger()
{
  const std::string filepath =
      fmt::format("{}{} {:%Y-%m-%d %Hh%Mm%Ss}.pcap", File::GetUserPath(D_DUMPSSL_IDX),
                  SConfig::GetInstance().GetGameID(), fmt::localtime(std::time(nullptr)));
  m_file = std::make_unique<Common::PCAP>(
      new File::IOFile(filepath, "wb", File::SharedAccess::Read), Common::PCAP::LinkType::Ethernet);
}

PCAPSSLCaptureLogger::~PCAPSSLCaptureLogger() = default;

void PCAPSSLCaptureLogger::OnNewSocket(s32 socket)
{
  m_read_sequence_number[socket] = 0;
  m_write_sequence_number[socket] = 0;
}

void PCAPSSLCaptureLogger::LogSSLRead(const void* data, std::size_t length, s32 socket)
{
  if (!Config::Get(Config::MAIN_NETWORK_SSL_DUMP_READ))
    return;
  Log(LogType::Read, data, length, socket, nullptr);
}

void PCAPSSLCaptureLogger::LogSSLWrite(const void* data, std::size_t length, s32 socket)
{
  if (!Config::Get(Config::MAIN_NETWORK_SSL_DUMP_WRITE))
    return;
  Log(LogType::Write, data, length, socket, nullptr);
}

void PCAPSSLCaptureLogger::LogRead(const void* data, std::size_t length, s32 socket, sockaddr* from)
{
  Log(LogType::Read, data, length, socket, from);
}

void PCAPSSLCaptureLogger::LogWrite(const void* data, std::size_t length, s32 socket, sockaddr* to)
{
  Log(LogType::Write, data, length, socket, to);
}

void PCAPSSLCaptureLogger::LogBBA(const void* data, std::size_t length)
{
  if (!Config::Get(Config::MAIN_NETWORK_DUMP_BBA))
    return;

  // Concurrency between CEXIETHERNET's RecvHandlePacket and SendFromDirectFIFO
  const std::lock_guard lock(m_io_mutex);
  m_file->AddPacket(static_cast<const u8*>(data), length);
}

void PCAPSSLCaptureLogger::Log(LogType log_type, const void* data, std::size_t length, s32 socket,
                               sockaddr* other)
{
  const auto state = Common::SaveNetworkErrorState();
  Common::ScopeGuard guard([&state] { Common::RestoreNetworkErrorState(state); });
  sockaddr_in sock;
  sockaddr_in peer;
  sockaddr_in* from;
  sockaddr_in* to;
  socklen_t sock_len = sizeof(sock);
  socklen_t peer_len = sizeof(sock);

  if (getsockname(socket, reinterpret_cast<sockaddr*>(&sock), &sock_len) != 0)
    return;

  if (other == nullptr && getpeername(socket, reinterpret_cast<sockaddr*>(&peer), &peer_len) != 0)
    return;

  if (log_type == LogType::Read)
  {
    from = other ? reinterpret_cast<sockaddr_in*>(other) : &peer;
    to = &sock;
  }
  else
  {
    from = &sock;
    to = other ? reinterpret_cast<sockaddr_in*>(other) : &peer;
  }

  LogIPv4(log_type, reinterpret_cast<const u8*>(data), static_cast<u16>(length), socket, *from,
          *to);
}

void PCAPSSLCaptureLogger::LogIPv4(LogType log_type, const u8* data, u16 length, s32 socket,
                                   const sockaddr_in& from, const sockaddr_in& to)
{
  int socket_type;
  socklen_t option_length = sizeof(int);

  if (getsockopt(socket, SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&socket_type),
                 &option_length) != 0 ||
      (socket_type != SOCK_STREAM && socket_type != SOCK_DGRAM))
  {
    return;
  }

  std::vector<u8> packet;
  auto insert = [&](const auto* new_data, std::size_t size) {
    const u8* begin = reinterpret_cast<const u8*>(new_data);
    packet.insert(packet.end(), begin, begin + size);
  };

  Common::EthernetHeader ethernet_header(0x800);
  auto mac = Common::StringToMacAddress(Config::Get(Config::MAIN_WIRELESS_MAC));
  if (mac)
  {
    auto& mac_address =
        (log_type == LogType::Write) ? ethernet_header.source : ethernet_header.destination;
    mac_address = *mac;
  }
  insert(&ethernet_header, ethernet_header.Size());

  if (socket_type == SOCK_STREAM)
  {
    u32& sequence_number = (log_type == LogType::Read) ? m_read_sequence_number[socket] :
                                                         m_write_sequence_number[socket];
    Common::TCPHeader tcp_header(from, to, sequence_number, data, length);
    sequence_number += static_cast<u32>(length);
    Common::IPv4Header ip_header(tcp_header.Size() + length, tcp_header.IPProto(), from, to);
    insert(&ip_header, ip_header.Size());
    insert(&tcp_header, tcp_header.Size());
  }
  else if (socket_type == SOCK_DGRAM)
  {
    Common::UDPHeader udp_header(from, to, length);
    Common::IPv4Header ip_header(udp_header.Size() + length, udp_header.IPProto(), from, to);
    insert(&ip_header, ip_header.Size());
    insert(&udp_header, udp_header.Size());
  }

  packet.insert(packet.end(), data, data + length);
  m_file->AddPacket(packet.data(), packet.size());
}

NetworkCaptureType PCAPSSLCaptureLogger::GetCaptureType() const
{
  return NetworkCaptureType::PCAP;
}
}  // namespace Core
