// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>

#ifdef _WIN32
#include <WinSock2.h>
using socklen_t = int;
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include "Common/CommonTypes.h"

namespace Common
{
class PCAP;
}

namespace Core
{
enum class NetworkCaptureType
{
  None,
  Raw,
  PCAP,
};

class NetworkCaptureLogger
{
public:
  NetworkCaptureLogger();
  NetworkCaptureLogger(const NetworkCaptureLogger&) = delete;
  NetworkCaptureLogger(NetworkCaptureLogger&&) = delete;
  NetworkCaptureLogger& operator=(const NetworkCaptureLogger&) = delete;
  NetworkCaptureLogger& operator=(NetworkCaptureLogger&&) = delete;
  virtual ~NetworkCaptureLogger();

  virtual void OnNewSocket(s32 socket) = 0;

  virtual void LogSSLRead(const void* data, std::size_t length, s32 socket) = 0;
  virtual void LogSSLWrite(const void* data, std::size_t length, s32 socket) = 0;

  virtual void LogRead(const void* data, std::size_t length, s32 socket, sockaddr* from) = 0;
  virtual void LogWrite(const void* data, std::size_t length, s32 socket, sockaddr* to) = 0;

  virtual void LogBBA(const void* data, std::size_t length) = 0;

  virtual NetworkCaptureType GetCaptureType() const = 0;
};

class DummyNetworkCaptureLogger : public NetworkCaptureLogger
{
public:
  void OnNewSocket(s32 socket) override;

  void LogSSLRead(const void* data, std::size_t length, s32 socket) override;
  void LogSSLWrite(const void* data, std::size_t length, s32 socket) override;

  void LogRead(const void* data, std::size_t length, s32 socket, sockaddr* from) override;
  void LogWrite(const void* data, std::size_t length, s32 socket, sockaddr* to) override;

  void LogBBA(const void* data, std::size_t length) override;

  NetworkCaptureType GetCaptureType() const override;
};

class BinarySSLCaptureLogger final : public DummyNetworkCaptureLogger
{
public:
  void LogSSLRead(const void* data, std::size_t length, s32 socket) override;
  void LogSSLWrite(const void* data, std::size_t length, s32 socket) override;

  NetworkCaptureType GetCaptureType() const override;
};

class PCAPSSLCaptureLogger final : public NetworkCaptureLogger
{
public:
  PCAPSSLCaptureLogger();
  ~PCAPSSLCaptureLogger();

  void OnNewSocket(s32 socket) override;

  void LogSSLRead(const void* data, std::size_t length, s32 socket) override;
  void LogSSLWrite(const void* data, std::size_t length, s32 socket) override;

  void LogRead(const void* data, std::size_t length, s32 socket, sockaddr* from) override;
  void LogWrite(const void* data, std::size_t length, s32 socket, sockaddr* to) override;

  void LogBBA(const void* data, std::size_t length) override;

  NetworkCaptureType GetCaptureType() const override;

private:
  enum class LogType
  {
    Read,
    Write,
  };

  void Log(LogType log_type, const void* data, std::size_t length, s32 socket, sockaddr* other);
  void LogIPv4(LogType log_type, const u8* data, u16 length, s32 socket, const sockaddr_in& from,
               const sockaddr_in& to);

  std::unique_ptr<Common::PCAP> m_file;
  std::mutex m_io_mutex;
  std::map<s32, u32> m_read_sequence_number;
  std::map<s32, u32> m_write_sequence_number;
};
}  // namespace Core
