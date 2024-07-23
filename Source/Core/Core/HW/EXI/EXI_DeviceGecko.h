// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <SFML/Network.hpp>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Core/HW/EXI/EXI_Device.h"

namespace ExpansionInterface
{
class GeckoSockServer
{
public:
  GeckoSockServer();
  ~GeckoSockServer();
  bool GetAvailableSock();

  // Client for this server object
  std::unique_ptr<sf::TcpSocket> client;
  void ClientThread();
  std::thread clientThread;
  std::mutex transfer_lock;

  std::deque<u8> send_fifo;
  std::deque<u8> recv_fifo;

private:
  static int client_count;
  Common::Flag client_running;

  // Only ever one server thread
  static void GeckoConnectionWaiter();

  static u16 server_port;
  static Common::Flag server_running;
  static std::thread connectionThread;
  static std::mutex connection_lock;
  static std::queue<std::unique_ptr<sf::TcpSocket>> waiting_socks;
};

class CEXIGecko : public IEXIDevice, private GeckoSockServer
{
public:
  explicit CEXIGecko(Core::System& system);
  bool IsPresent() const override { return true; }
  void ImmReadWrite(u32& _uData, u32 _uSize) override;

private:
  enum
  {
    CMD_LED_OFF = 0x7,
    CMD_LED_ON = 0x8,
    CMD_INIT = 0x9,
    CMD_RECV = 0xa,
    CMD_SEND = 0xb,
    CMD_CHK_TX = 0xc,
    CMD_CHK_RX = 0xd,
  };

  static constexpr u32 ident = 0x04700000;
};
}  // namespace ExpansionInterface
