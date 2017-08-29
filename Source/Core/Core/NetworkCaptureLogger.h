// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/File.h"

class PCAP;

namespace Core
{
class NetworkCaptureLogger
{
public:
  virtual ~NetworkCaptureLogger();

  virtual void LogSSLRead(const void* data, std::size_t length, s32 socket) = 0;
  virtual void LogSSLWrite(const void* data, std::size_t length, s32 socket) = 0;

  virtual void LogTCPRead(const void* data, std::size_t length, s32 socket) = 0;
  virtual void LogTCPWrite(const void* data, std::size_t length, s32 socket) = 0;

  virtual void LogUDPRead(const void* data, std::size_t length, s32 socket) = 0;
  virtual void LogUDPWrite(const void* data, std::size_t length, s32 socket) = 0;

  virtual void LogRead(const void* data, std::size_t length, s32 socket) = 0;
  virtual void LogWrite(const void* data, std::size_t length, s32 socket) = 0;
};

class DummyNetworkCaptureLogger : public NetworkCaptureLogger
{
public:
  void LogSSLRead(const void* data, std::size_t length, s32 socket) override;
  void LogSSLWrite(const void* data, std::size_t length, s32 socket) override;

  void LogTCPRead(const void* data, std::size_t length, s32 socket) override;
  void LogTCPWrite(const void* data, std::size_t length, s32 socket) override;

  void LogUDPRead(const void* data, std::size_t length, s32 socket) override;
  void LogUDPWrite(const void* data, std::size_t length, s32 socket) override;

  void LogRead(const void* data, std::size_t length, s32 socket) override;
  void LogWrite(const void* data, std::size_t length, s32 socket) override;
};

class BinarySSLCaptureLogger : public DummyNetworkCaptureLogger
{
public:
  BinarySSLCaptureLogger();

  void LogSSLRead(const void* data, std::size_t length, s32 socket) override;
  void LogSSLWrite(const void* data, std::size_t length, s32 socket) override;

private:
  File::IOFile m_file_read;
  File::IOFile m_file_write;
};

class PCAPSSLCaptureLogger : public NetworkCaptureLogger
{
public:
  PCAPSSLCaptureLogger();
  ~PCAPSSLCaptureLogger();

  void LogSSLRead(const void* data, std::size_t length, s32 socket) override;
  void LogSSLWrite(const void* data, std::size_t length, s32 socket) override;

  void LogTCPRead(const void* data, std::size_t length, s32 socket) override;
  void LogTCPWrite(const void* data, std::size_t length, s32 socket) override;

  void LogUDPRead(const void* data, std::size_t length, s32 socket) override;
  void LogUDPWrite(const void* data, std::size_t length, s32 socket) override;

  void LogRead(const void* data, std::size_t length, s32 socket) override;
  void LogWrite(const void* data, std::size_t length, s32 socket) override;

private:
  enum class LogType
  {
    TCP,
    UDP,
    SSL
  };

  static constexpr std::size_t ETHERNET_HEADER_SIZE = 14;
  static constexpr std::size_t IP_HEADER_SIZE = 20;
  static constexpr std::size_t TCP_HEADER_SIZE = 20;
  static constexpr std::size_t UDP_HEADER_SIZE = 8;

  using EthernetHeader = std::array<u8, ETHERNET_HEADER_SIZE>;
  using IPHeader = std::array<u8, IP_HEADER_SIZE>;
  using TCPHeader = std::array<u8, TCP_HEADER_SIZE>;
  using UDPHeader = std::array<u8, UDP_HEADER_SIZE>;

  EthernetHeader GetEthernetHeader(bool is_sender) const;
  IPHeader GetIPHeader(bool is_sender, std::size_t data_size, u8 protocol, s32 socket) const;

  struct ErrorState
  {
    int error;
#ifdef _WIN32
    int wsa_error;
#endif
  };
  ErrorState SaveState() const;
  void RestoreState(const ErrorState& state) const;

  void Log(LogType log_type, bool is_sender, const void* data, std::size_t length, s32 socket);
  void LogTCP(bool is_sender, const void* data, std::size_t length, s32 socket);
  void LogUDP(bool is_sender, const void* data, std::size_t length, s32 socket);

  std::unique_ptr<PCAP> m_file;
  u32 m_read_sequence_number = 0;
  u32 m_write_sequence_number = 0;
};
}  // namespace Core
