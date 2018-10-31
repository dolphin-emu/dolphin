// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/EXI/EXI_DeviceGecko.h"

#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
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

  if (client_count <= 0)
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
    server_running.Set(server.listen(server_port) == sf::Socket::Done);
    if (!server_running.IsSet())
      server_port++;
  }

  if (!server_running.IsSet())
    return;

  Core::DisplayMessage(StringFromFormat("USBGecko: Listening on TCP port %u", server_port), 5000);

  server.setBlocking(false);

  auto new_client = std::make_unique<sf::TcpSocket>();
  while (server_running.IsSet())
  {
    if (server.accept(*new_client) == sf::Socket::Done)
    {
      std::lock_guard<std::mutex> lk(connection_lock);
      waiting_socks.push(std::move(new_client));

      new_client = std::make_unique<sf::TcpSocket>();
    }

    Common::SleepCurrentThread(1);
  }
}

bool GeckoSockServer::GetAvailableSock()
{
  bool sock_filled = false;

  std::lock_guard<std::mutex> lk(connection_lock);

  if (!waiting_socks.empty())
  {
    client = std::move(waiting_socks.front());
    if (clientThread.joinable())
    {
      client_running.Clear();
      clientThread.join();

      recv_fifo = std::deque<u8>();
      send_fifo = std::deque<u8>();
    }
    clientThread = std::thread(&GeckoSockServer::ClientThread, this);
    client_count++;
    waiting_socks.pop();
    sock_filled = true;
  }

  return sock_filled;
}

void GeckoSockServer::ClientThread()
{
  client_running.Set();

  Common::SetCurrentThreadName("Gecko Client");

  client->setBlocking(false);

  while (client_running.IsSet())
  {
    bool did_nothing = true;

    {
      std::lock_guard<std::mutex> lk(transfer_lock);

      // what's an ideal buffer size?
      char data[128];
      std::size_t got = 0;

      if (client->receive(&data[0], ArraySize(data), got) == sf::Socket::Disconnected)
        client_running.Clear();

      if (got != 0)
      {
        did_nothing = false;

        recv_fifo.insert(recv_fifo.end(), &data[0], &data[got]);
      }

      if (!send_fifo.empty())
      {
        did_nothing = false;

        std::vector<char> packet(send_fifo.begin(), send_fifo.end());
        send_fifo.clear();

        if (client->send(&packet[0], packet.size()) == sf::Socket::Disconnected)
          client_running.Clear();
      }
    }  // unlock transfer

    if (did_nothing)
      Common::YieldCPU();
  }

  client->disconnect();
}

void CEXIGecko::ImmReadWrite(u32& _uData, u32 _uSize)
{
  // We don't really care about _uSize
  (void)_uSize;

  if (!client || client->getLocalPort() == 0)
    GetAvailableSock();

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
    std::lock_guard<std::mutex> lk(transfer_lock);
    if (!recv_fifo.empty())
    {
      _uData = 0x08000000 | (recv_fifo.front() << 16);
      recv_fifo.pop_front();
    }
    break;
  }

  // Gecko -> PC
  // |= 0x04000000 if successful
  case CMD_SEND:
  {
    std::lock_guard<std::mutex> lk(transfer_lock);
    send_fifo.push_back(_uData >> 20);
    _uData = 0x04000000;
    break;
  }

  // Check if ok for Gecko -> PC, or FIFO full
  // |= 0x04000000 if FIFO is not full
  case CMD_CHK_TX:
    _uData = 0x04000000;
    break;

  // Check if data in FIFO for PC -> Gecko, or FIFO empty
  // |= 0x04000000 if data in recv FIFO
  case CMD_CHK_RX:
  {
    std::lock_guard<std::mutex> lk(transfer_lock);
    _uData = recv_fifo.empty() ? 0 : 0x04000000;
    break;
  }

  default:
    ERROR_LOG(EXPANSIONINTERFACE, "Unknown USBGecko command %x", _uData);
    break;
  }
}
}  // namespace ExpansionInterface
