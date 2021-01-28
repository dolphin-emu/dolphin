// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/NetworkCaptureLogger.h"

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"

namespace Core
{
NetworkCaptureLogger::NetworkCaptureLogger() = default;
NetworkCaptureLogger::~NetworkCaptureLogger() = default;

void DummyNetworkCaptureLogger::LogRead(const void* data, std::size_t length)
{
}

void DummyNetworkCaptureLogger::LogWrite(const void* data, std::size_t length)
{
}

NetworkCaptureType DummyNetworkCaptureLogger::GetCaptureType() const
{
  return NetworkCaptureType::None;
}

void BinarySSLCaptureLogger::LogRead(const void* data, std::size_t length)
{
  if (!Config::Get(Config::MAIN_NETWORK_SSL_DUMP_READ))
    return;
  const std::string filename =
      File::GetUserPath(D_DUMPSSL_IDX) + SConfig::GetInstance().GetGameID() + "_read.bin";
  File::IOFile(filename, "ab").WriteBytes(data, length);
}

void BinarySSLCaptureLogger::LogWrite(const void* data, std::size_t length)
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
}  // namespace Core
