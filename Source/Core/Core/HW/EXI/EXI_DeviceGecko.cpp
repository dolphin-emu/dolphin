// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceGecko.h"

#include <array>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/Core.h"

namespace ExpansionInterface
{
u16 GeckoSockServer::server_port;
int GeckoSockServer::client_count;
std::thread GeckoSockServer::connectionThread;
Common::Flag GeckoSockServer::server_running;
std::mutex GeckoSockServer::connection_lock;
std::queue<std::unique_ptr<sf::TcpSocket>> GeckoSockServer::waiting_socks;

GeckoSockServer::GeckoSockServer() : client_running(false)
{
  if (!connectionThread.joinable())
    connectionThread = std::thread(GeckoConnectionWaiter);
}

GeckoSockServer::~GeckoSockServer()
{
  if (clientThread.joinable())
  {
    --client_count;

    client_running.Clear();
    clientThread.join();
  }

  if (client_count <= 0 && connectionThread.joinable())
  {
    server_running.Clear();
    connectionThread.join();
  }
}

void GeckoSockServer::GeckoConnectionWaiter()
{
  Common::SetCurrentThreadName("Gecko Connection Waiter");

  sf::TcpListener server;
  server_port = 0xd6ec;  // "dolphin gecko"
  for (int bind_tries = 0; bind_tries <= 10 && !server_running.IsSet(); bind_tries++)
  {
    server_running.Set(server.listen(server_port) == sf::Socket::Status::Done);
    if (!server_running.IsSet())
      server_port++;
  }

  if (!server_running.IsSet())
    return;

  Core::DisplayMessage(fmt::format("USBGecko: Listening on TCP port {}", server_port), 5000);

  server.setBlocking(false);

  auto new_client = std::make_unique<sf::TcpSocket>();
  while (server_running.IsSet())
  {
    if (server.accept(*new_client) == sf::Socket::Status::Done)
    {
      std::lock_guard lk(connection_lock);
      waiting_socks.push(std::move(new_client));

      new_client = std::make_unique<sf::TcpSocket>();
    }

    Common::SleepCurrentThread(1);
  }
}

bool GeckoSockServer::GetAvailableSock()
{
  bool sock_filled = false;

  std::lock_guard lk(connection_lock);

  if (!waiting_socks.empty())
  {
    if (clientThread.joinable())
    {
      client_running.Clear();
      clientThread.join();

      recv_fifo = std::deque<u8>();
      send_fifo = std::deque<u8>();
    }
    client = std::move(waiting_socks.front());
    client_running.Set();
    client_connected.Set();
    clientThread = std::thread(&GeckoSockServer::ClientThread, this);
    client_count++;
    waiting_socks.pop();
    sock_filled = true;
  }

  return sock_filled;
}

void GeckoSockServer::ClientThread()
{
  Common::SetCurrentThreadName("Gecko Client");

  client->setBlocking(false);

  while (client_running.IsSet())
  {
    bool did_nothing = true;

    {
      std::lock_guard lk(recv_lock);

      // what's an ideal buffer size?
      std::array<char, 128> buffer;
      std::size_t got = 0;

      if (client->receive(buffer.data(), buffer.size(), got) == sf::Socket::Status::Disconnected)
        client_running.Clear();

      if (got != 0)
      {
        did_nothing = false;

        recv_fifo.insert(recv_fifo.end(), buffer.data(), buffer.data() + got);
      }
    }  // unlock recv

    {
      std::lock_guard lk(send_lock);

      if (!send_fifo.empty())
      {
        std::size_t sent;

        std::vector<char> packet(send_fifo.begin(), send_fifo.end());

        if (client->send(packet.data(), packet.size(), sent) == sf::Socket::Status::Disconnected)
          client_running.Clear();

        if (sent)
        {
          did_nothing = false;

          send_fifo.erase(send_fifo.begin(), send_fifo.begin() + sent);
        }
      }
    }  // unlock send

    if (did_nothing)
      Common::YieldCPU();
  }

  client->disconnect();
  client_connected.Clear();
}

CEXIGecko::CEXIGecko(Core::System& system) : IEXIDevice(system)
{
}

void CEXIGecko::ImmReadWrite(u32& _uData, u32 _uSize)
{
  // We don't really care about _uSize
  (void)_uSize;

  if (!client_connected.IsSet())
  {
    if (GetAvailableSock())
      m_recv_buffer.clear();
  }

  switch (_uData >> 28)
  {
  case CMD_LED_OFF:
    Core::DisplayMessage("USBGecko: No LEDs for you!", 3000);
    break;
  case CMD_LED_ON:
    Core::DisplayMessage("USBGecko: A piercing blue light is now shining in your general direction",
                         3000);
    break;

  case CMD_INIT:
    _uData = ident;
    break;

  // PC -> Gecko
  // |= 0x08000000 if successful
  case CMD_RECV:
  {
    if (m_recv_buffer.empty())
    {
      std::lock_guard lk(recv_lock);
      m_recv_buffer.swap(recv_fifo);
    }
    if (!m_recv_buffer.empty())
    {
      _uData = 0x08000000 | (m_recv_buffer.front() << 16);
      m_recv_buffer.pop_front();
    }
    break;
  }

  // Gecko -> PC
  // |= 0x04000000 if successful
  case CMD_SEND:
  {
    std::lock_guard lk(send_lock);

    if (send_fifo.size() < 512)
    {
      send_fifo.push_back(_uData >> 20);
      _uData = 0x04000000;
    }
    else
    {
      _uData = 0;
    }

    break;
  }

  // Check if ok for Gecko -> PC, or FIFO full
  // |= 0x04000000 if FIFO is not full
  case CMD_CHK_TX:
  {
    std::lock_guard lk(send_lock);
    _uData = send_fifo.size() < 512 ? 0x04000000 : 0;
    break;
  }

  // Check if data in FIFO for PC -> Gecko, or FIFO empty
  // |= 0x04000000 if data in recv FIFO
  case CMD_CHK_RX:
  {
    std::lock_guard lk(recv_lock);
    _uData = m_recv_buffer.empty() && recv_fifo.empty() ? 0 : 0x04000000;
    break;
  }

  default:
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "Unknown USBGecko command {:x}", _uData);
    break;
  }
}
}  // namespace ExpansionInterface
