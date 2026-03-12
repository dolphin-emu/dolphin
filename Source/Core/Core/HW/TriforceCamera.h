// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include "Common/HttpServer.h"

class TriforceCameraInstance
{
public:
  TriforceCameraInstance();

  TriforceCameraInstance(TriforceCameraInstance&) = delete;
  TriforceCameraInstance& operator=(TriforceCameraInstance&) = delete;
  TriforceCameraInstance(const TriforceCameraInstance&) = delete;
  TriforceCameraInstance& operator=(const TriforceCameraInstance&) = delete;

  std::optional<Common::IPv4Port> GetAddress() const;

  void Recreate();

private:
  std::optional<Common::HttpServer> m_http_server;
  std::optional<Common::IPv4Port> m_redirection_address;
};

namespace TriforceCamera
{
void Init();
void Shutdown();

TriforceCameraInstance& GetInstance();
bool IsInitialized();
}  // namespace TriforceCamera
