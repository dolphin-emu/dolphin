// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include "Common/File.h"

namespace Core
{
class NetworkCaptureLogger
{
public:
  virtual ~NetworkCaptureLogger();
  virtual void LogRead(const void* data, std::size_t length) = 0;
  virtual void LogWrite(const void* data, std::size_t length) = 0;
};

class DummyNetworkCaptureLogger : public NetworkCaptureLogger
{
public:
  void LogRead(const void* data, std::size_t length) override;
  void LogWrite(const void* data, std::size_t length) override;
};

class BinarySSLCaptureLogger : public NetworkCaptureLogger
{
public:
  BinarySSLCaptureLogger();

  void LogRead(const void* data, std::size_t length) override;
  void LogWrite(const void* data, std::size_t length) override;

private:
  File::IOFile m_file_read;
  File::IOFile m_file_write;
};
}  // namespace Core
