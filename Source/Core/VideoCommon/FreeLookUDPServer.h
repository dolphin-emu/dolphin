// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include <SFML/Network/UdpSocket.hpp>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"

class FreeLookUDPServer
{
public:
  FreeLookUDPServer(std::string address, u16 port);

  void Update();

  Common::Matrix44 GetView() const { return m_view; }
  Common::Vec2 GetFovMultiplier() const { return m_fov_multiplier; }

private:
  void ProcessReceivedData(const u8* data, std::size_t packet_size);

  sf::UdpSocket m_socket;

  Common::Matrix44 m_view = Common::Matrix44::Identity();
  Common::Vec2 m_fov_multiplier = Common::Vec2{1, 1};
};
