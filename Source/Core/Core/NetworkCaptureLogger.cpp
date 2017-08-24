// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/NetworkCaptureLogger.h"

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"

namespace Core
{
NetworkCaptureLogger::~NetworkCaptureLogger() = default;

void DummyNetworkCaptureLogger::LogRead(const void* data, std::size_t length)
{
}

void DummyNetworkCaptureLogger::LogWrite(const void* data, std::size_t length)
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

void BinarySSLCaptureLogger::LogRead(const void* data, std::size_t length)
{
  m_file_read.WriteBytes(data, length);
}

void BinarySSLCaptureLogger::LogWrite(const void* data, std::size_t length)
{
  m_file_write.WriteBytes(data, length);
}
}  // namespace Core
