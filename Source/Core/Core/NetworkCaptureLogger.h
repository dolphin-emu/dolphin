// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

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

  virtual void LogRead(const void* data, std::size_t length) = 0;
  virtual void LogWrite(const void* data, std::size_t length) = 0;
  virtual NetworkCaptureType GetCaptureType() const = 0;
};

class DummyNetworkCaptureLogger : public NetworkCaptureLogger
{
public:
  void LogRead(const void* data, std::size_t length) override;
  void LogWrite(const void* data, std::size_t length) override;
  NetworkCaptureType GetCaptureType() const override;
};

class BinarySSLCaptureLogger final : public NetworkCaptureLogger
{
public:
  void LogRead(const void* data, std::size_t length) override;
  void LogWrite(const void* data, std::size_t length) override;
  NetworkCaptureType GetCaptureType() const override;
};
}  // namespace Core
