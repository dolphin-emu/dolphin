// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/SI/SI_DeviceGBA.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include <SFML/Network.hpp>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SystemTimers.h"

namespace SerialInterface
{
namespace
{
std::thread s_connection_thread;
std::queue<std::unique_ptr<sf::TcpSocket>> s_waiting_socks;
std::queue<std::unique_ptr<sf::TcpSocket>> s_waiting_clocks;
std::mutex s_cs_gba;
std::mutex s_cs_gba_clk;
u8 s_num_connected;
Common::Flag s_server_running;
}

enum EJoybusCmds
{
  CMD_RESET = 0xff,
  CMD_STATUS = 0x00,
  CMD_READ = 0x14,
  CMD_WRITE = 0x15
};

const u64 BITS_PER_SECOND = 115200;
const u64 BYTES_PER_SECOND = BITS_PER_SECOND / 8;

u8 GetNumConnected()
{
  int num_ports_connected = s_num_connected;
  if (num_ports_connected == 0)
    num_ports_connected = 1;

  return num_ports_connected;
}

// --- GameBoy Advance "Link Cable" ---

int GetTransferTime(u8 cmd)
{
  u64 bytes_transferred = 0;

  switch (cmd)
  {
  case CMD_RESET:
  case CMD_STATUS:
  {
    bytes_transferred = 4;
    break;
  }
  case CMD_READ:
  {
    bytes_transferred = 6;
    break;
  }
  case CMD_WRITE:
  {
    bytes_transferred = 1;
    break;
  }
  default:
  {
    bytes_transferred = 1;
    break;
  }
  }
  return (int)(bytes_transferred * SystemTimers::GetTicksPerSecond() /
               (GetNumConnected() * BYTES_PER_SECOND));
}

static void GBAConnectionWaiter()
{
  s_server_running.Set();

  Common::SetCurrentThreadName("GBA Connection Waiter");

  sf::TcpListener server;
  sf::TcpListener clock_server;

  // "dolphin gba"
  if (server.listen(0xd6ba) != sf::Socket::Done)
    return;

  // "clock"
  if (clock_server.listen(0xc10c) != sf::Socket::Done)
    return;

  server.setBlocking(false);
  clock_server.setBlocking(false);

  auto new_client = std::make_unique<sf::TcpSocket>();
  while (s_server_running.IsSet())
  {
    if (server.accept(*new_client) == sf::Socket::Done)
    {
      std::lock_guard<std::mutex> lk(s_cs_gba);
      s_waiting_socks.push(std::move(new_client));

      new_client = std::make_unique<sf::TcpSocket>();
    }
    if (clock_server.accept(*new_client) == sf::Socket::Done)
    {
      std::lock_guard<std::mutex> lk(s_cs_gba_clk);
      s_waiting_clocks.push(std::move(new_client));

      new_client = std::make_unique<sf::TcpSocket>();
    }

    Common::SleepCurrentThread(1);
  }
}

void GBAConnectionWaiter_Shutdown()
{
  s_server_running.Clear();
  if (s_connection_thread.joinable())
    s_connection_thread.join();
}

static bool GetAvailableSock(std::unique_ptr<sf::TcpSocket>& sock_to_fill)
{
  bool sock_filled = false;

  std::lock_guard<std::mutex> lk(s_cs_gba);

  if (!s_waiting_socks.empty())
  {
    sock_to_fill = std::move(s_waiting_socks.front());
    s_waiting_socks.pop();
    sock_filled = true;
  }

  return sock_filled;
}

static bool GetNextClock(std::unique_ptr<sf::TcpSocket>& sock_to_fill)
{
  bool sock_filled = false;

  std::lock_guard<std::mutex> lk(s_cs_gba_clk);

  if (!s_waiting_clocks.empty())
  {
    sock_to_fill = std::move(s_waiting_clocks.front());
    s_waiting_clocks.pop();
    sock_filled = true;
  }

  return sock_filled;
}

GBASockServer::GBASockServer()
{
  if (!s_connection_thread.joinable())
    s_connection_thread = std::thread(GBAConnectionWaiter);

  s_num_connected = 0;
}

GBASockServer::~GBASockServer()
{
  Disconnect();
}

void GBASockServer::Disconnect()
{
  if (m_client)
  {
    s_num_connected--;
    m_client->disconnect();
    m_client = nullptr;
  }
  if (m_clock_sync)
  {
    m_clock_sync->disconnect();
    m_clock_sync = nullptr;
  }
  m_last_time_slice = 0;
  m_booted = false;
}

void GBASockServer::ClockSync()
{
  if (!m_clock_sync)
    if (!GetNextClock(m_clock_sync))
      return;

  u32 time_slice = 0;

  if (m_last_time_slice == 0)
  {
    s_num_connected++;
    m_last_time_slice = CoreTiming::GetTicks();
    time_slice = (u32)(SystemTimers::GetTicksPerSecond() / 60);
  }
  else
  {
    time_slice = (u32)(CoreTiming::GetTicks() - m_last_time_slice);
  }

  time_slice = (u32)((u64)time_slice * 16777216 / SystemTimers::GetTicksPerSecond());
  m_last_time_slice = CoreTiming::GetTicks();
  char bytes[4] = {0, 0, 0, 0};
  bytes[0] = (time_slice >> 24) & 0xff;
  bytes[1] = (time_slice >> 16) & 0xff;
  bytes[2] = (time_slice >> 8) & 0xff;
  bytes[3] = time_slice & 0xff;

  sf::Socket::Status status = m_clock_sync->send(bytes, 4);
  if (status == sf::Socket::Disconnected)
  {
    m_clock_sync->disconnect();
    m_clock_sync = nullptr;
  }
}

bool GBASockServer::Connect()
{
  if (!IsConnected())
    GetAvailableSock(m_client);
  return IsConnected();
}

bool GBASockServer::IsConnected()
{
  return static_cast<bool>(m_client);
}

void GBASockServer::Send(const u8* si_buffer)
{
  if (!Connect())
    return;

  std::array<u8, 5> send_data;
  for (size_t i = 0; i < send_data.size(); i++)
    send_data[i] = si_buffer[i ^ 3];

  u8 cmd = send_data[0];
  if (cmd != CMD_STATUS)
    m_booted = true;

  m_client->setBlocking(false);
  sf::Socket::Status status;
  if (cmd == CMD_WRITE)
    status = m_client->send(send_data.data(), send_data.size());
  else
    status = m_client->send(send_data.data(), 1);

  if (status == sf::Socket::Disconnected)
    Disconnect();
}

int GBASockServer::Receive(u8* si_buffer)
{
  if (!m_client)
    if (!GetAvailableSock(m_client))
      return 5;

  if (m_booted)
  {
    sf::SocketSelector selector;
    selector.add(*m_client);
    selector.wait(sf::milliseconds(1000));
  }

  size_t num_received = 0;
  std::array<u8, 5> recv_data;
  sf::Socket::Status recv_stat =
      m_client->receive(recv_data.data(), recv_data.size(), num_received);
  if (recv_stat == sf::Socket::Disconnected)
  {
    Disconnect();
    return 5;
  }

  if (recv_stat == sf::Socket::NotReady)
    num_received = 0;

  if (num_received > recv_data.size())
    num_received = recv_data.size();

  if (num_received > 0)
  {
    for (size_t i = 0; i < recv_data.size(); i++)
      si_buffer[i ^ 3] = recv_data[i];
  }

  return static_cast<int>(num_received);
}

CSIDevice_GBA::CSIDevice_GBA(SIDevices device, int device_number) : ISIDevice(device, device_number)
{
}

CSIDevice_GBA::~CSIDevice_GBA()
{
  GBASockServer::Disconnect();
}

int CSIDevice_GBA::RunBuffer(u8* buffer, int length)
{
  switch (m_next_action)
  {
  case NextAction::SendCommand:
  {
    ClockSync();
    if (Connect())
    {
#ifdef _DEBUG
      NOTICE_LOG(SERIALINTERFACE, "%01d cmd %02x [> %02x%02x%02x%02x]", m_device_number, buffer[3],
                 buffer[2], buffer[1], buffer[0], buffer[7]);
#endif
      Send(buffer);
    }
    m_last_cmd = buffer[3];
    m_timestamp_sent = CoreTiming::GetTicks();
    m_next_action = NextAction::WaitTransferTime;
  }

  // [[fallthrough]]
  case NextAction::WaitTransferTime:
  {
    int elapsed_time = static_cast<int>(CoreTiming::GetTicks() - m_timestamp_sent);
    // Tell SI to ask again after TransferInterval() cycles
    if (GetTransferTime(m_last_cmd) > elapsed_time)
      return 0;
    m_next_action = NextAction::ReceiveResponse;
  }

  // [[fallthrough]]
  case NextAction::ReceiveResponse:
  {
    int num_data_received = Receive(buffer);
    if (IsConnected())
    {
#ifdef _DEBUG
      LogTypes::LOG_LEVELS log_level = (m_last_cmd == CMD_STATUS || m_last_cmd == CMD_RESET) ?
                                           LogTypes::LERROR :
                                           LogTypes::LWARNING;
      GENERIC_LOG(LogTypes::SERIALINTERFACE, log_level,
                  "%01d                              [< %02x%02x%02x%02x%02x] (%i)",
                  m_device_number, buffer[3], buffer[2], buffer[1], buffer[0], buffer[7],
                  num_data_received);
#endif
    }
    if (num_data_received > 0)
      m_next_action = NextAction::SendCommand;
    return num_data_received;
  }
  }

  // This should never happen, but appease MSVC which thinks it might.
  ERROR_LOG(SERIALINTERFACE, "Unknown state %i\n", static_cast<int>(m_next_action));
  return 0;
}

int CSIDevice_GBA::TransferInterval()
{
  return GetTransferTime(m_last_cmd);
}

bool CSIDevice_GBA::GetData(u32& hi, u32& low)
{
  return false;
}

void CSIDevice_GBA::SendCommand(u32 command, u8 poll)
{
}
}  // namespace SerialInterface
