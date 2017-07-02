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

GBASockServer::GBASockServer(int device_number) : m_device_number{device_number}
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

void GBASockServer::Send(const u8* si_buffer)
{
  if (!m_client)
    if (!GetAvailableSock(m_client))
      return;

  for (size_t i = 0; i < m_send_data.size(); i++)
    m_send_data[i] = si_buffer[i ^ 3];

  m_cmd = (u8)m_send_data[0];

#ifdef _DEBUG
  NOTICE_LOG(SERIALINTERFACE, "%01d cmd %02x [> %02x%02x%02x%02x]", m_device_number,
             (u8)m_send_data[0], (u8)m_send_data[1], (u8)m_send_data[2], (u8)m_send_data[3],
             (u8)m_send_data[4]);
#endif

  m_client->setBlocking(false);
  sf::Socket::Status status;
  if (m_cmd == CMD_WRITE)
    status = m_client->send(m_send_data.data(), m_send_data.size());
  else
    status = m_client->send(m_send_data.data(), 1);

  if (m_cmd != CMD_STATUS)
    m_booted = true;

  if (status == sf::Socket::Disconnected)
    Disconnect();

  m_time_cmd_sent = CoreTiming::GetTicks();
}

int GBASockServer::Receive(u8* si_buffer)
{
  if (!m_client)
    if (!GetAvailableSock(m_client))
      return 5;

  size_t num_received = 0;
  u64 transfer_time = GetTransferTime((u8)m_send_data[0]);
  bool block = (CoreTiming::GetTicks() - m_time_cmd_sent) > transfer_time;
  if (m_cmd == CMD_STATUS && !m_booted)
    block = false;

  if (block)
  {
    sf::SocketSelector selector;
    selector.add(*m_client);
    selector.wait(sf::milliseconds(1000));
  }

  sf::Socket::Status recv_stat =
      m_client->receive(m_recv_data.data(), m_recv_data.size(), num_received);
  if (recv_stat == sf::Socket::Disconnected)
  {
    Disconnect();
    return 5;
  }

  if (recv_stat == sf::Socket::NotReady)
    num_received = 0;

  if (num_received > m_recv_data.size())
    num_received = m_recv_data.size();

  if (num_received > 0)
  {
#ifdef _DEBUG
    if ((u8)m_send_data[0] == 0x00 || (u8)m_send_data[0] == 0xff)
    {
      WARN_LOG(SERIALINTERFACE, "%01d                              [< %02x%02x%02x%02x%02x] (%zu)",
               m_device_number, (u8)m_recv_data[0], (u8)m_recv_data[1], (u8)m_recv_data[2],
               (u8)m_recv_data[3], (u8)m_recv_data[4], num_received);
    }
    else
    {
      ERROR_LOG(SERIALINTERFACE, "%01d                              [< %02x%02x%02x%02x%02x] (%zu)",
                m_device_number, (u8)m_recv_data[0], (u8)m_recv_data[1], (u8)m_recv_data[2],
                (u8)m_recv_data[3], (u8)m_recv_data[4], num_received);
    }
#endif

    for (size_t i = 0; i < m_recv_data.size(); i++)
      si_buffer[i ^ 3] = m_recv_data[i];
  }

  return (int)num_received;
}

CSIDevice_GBA::CSIDevice_GBA(SIDevices device, int device_number)
    : ISIDevice(device, device_number), GBASockServer(device_number)
{
}

CSIDevice_GBA::~CSIDevice_GBA()
{
  GBASockServer::Disconnect();
}

int CSIDevice_GBA::RunBuffer(u8* buffer, int length)
{
  if (!m_waiting_for_response)
  {
    for (size_t i = 0; i < m_send_data.size(); i++)
      m_send_data[i] = buffer[i ^ 3];

    m_num_data_received = 0;
    ClockSync();
    Send(buffer);
    m_timestamp_sent = CoreTiming::GetTicks();
    m_waiting_for_response = true;
  }

  if (m_waiting_for_response && m_num_data_received == 0)
  {
    m_num_data_received = Receive(buffer);
  }

  if ((GetTransferTime(m_send_data[0])) > (int)(CoreTiming::GetTicks() - m_timestamp_sent))
  {
    return 0;
  }
  else
  {
    if (m_num_data_received != 0)
      m_waiting_for_response = false;
    return m_num_data_received;
  }
}

int CSIDevice_GBA::TransferInterval()
{
  return GetTransferTime(m_send_data[0]);
}

bool CSIDevice_GBA::GetData(u32& hi, u32& low)
{
  return false;
}

void CSIDevice_GBA::SendCommand(u32 command, u8 poll)
{
}
}  // namespace SerialInterface
