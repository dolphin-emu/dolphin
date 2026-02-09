// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include "Common/HttpServer.h"

namespace Triforce
{

class Camera final
{
public:
  static Camera& GetInstance()
  {
    static Camera instance;
    return instance;
  }

  Camera() {}

  Camera(Camera&) = delete;
  Camera& operator=(Camera&) = delete;
  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

  std::optional<Common::IPv4Port> GetAddress() const;

  bool IsInitialized();

  void Init();
  void Shutdown();
  void Recreate();

private:
  std::optional<Common::HttpServer> m_http_server;
  std::optional<Common::IPv4Port> m_redirection_address;
};

}  // namespace Triforce
