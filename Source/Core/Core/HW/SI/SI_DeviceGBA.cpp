// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/SI/SI_DeviceGBA.h"

#include <memory>
#include <stop_token>
#include <thread>

#include <asio.hpp>

#include "Common/AsioUtil.h"  // IWYU pragma: keep
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/SPSCQueue.h"
#include "Common/Swap.h"
#include "Common/Thread.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

namespace
{

constexpr u16 GBA_DATA_PORT = 0xd6ba;   // "dolphin gba"
constexpr u16 GBA_CLOCK_PORT = 0xc10c;  // "clock"

constexpr auto SEND_MAX_SIZE = 5uz;
constexpr auto RECV_MAX_SIZE = 5uz;

using asio::ip::tcp;

// Accepts incoming connections and runs IO context thread.
class GBAConnectionHandler final
{
public:
  // All 4 GBAs share one "Connection Handler".
  static std::shared_ptr<GBAConnectionHandler> GetInstance()
  {
    auto result = s_instance.lock();
    if (!result)
    {
      result = std::shared_ptr<GBAConnectionHandler>(new GBAConnectionHandler);
      s_instance = result;
    }
    return result;
  }

  std::optional<tcp::socket> GetNextSock()
  {
    std::optional<tcp::socket> result;
    if (!m_waiting_socks.Empty())
    {
      result = std::move(m_waiting_socks.Front());
      m_waiting_socks.Pop();
    }
    return result;
  }

  std::optional<tcp::socket> GetNextClock()
  {
    std::optional<tcp::socket> result;
    if (!m_waiting_clocks.Empty())
    {
      result = std::move(m_waiting_clocks.Front());
      m_waiting_clocks.Pop();
    }
    return result;
  }

private:
  GBAConnectionHandler() = default;

  static std::weak_ptr<GBAConnectionHandler> s_instance;

  void ThreadFunc(std::stop_token stoken);

  asio::io_context m_io;

  Common::SPSCQueue<tcp::socket> m_waiting_socks;
  Common::SPSCQueue<tcp::socket> m_waiting_clocks;

  std::jthread m_connection_thread{std::bind_front(&GBAConnectionHandler::ThreadFunc, this)};
};

std::weak_ptr<GBAConnectionHandler> GBAConnectionHandler::s_instance;

void GBAConnectionHandler::ThreadFunc(std::stop_token stoken)
{
  Common::SetCurrentThreadName("GBA IO Context");

  tcp::acceptor data_acceptor{m_io};
  tcp::acceptor clock_acceptor{m_io};
  {
    std::error_code ec;
    (void)data_acceptor.open(tcp::v4(), ec);
    (void)clock_acceptor.open(tcp::v4(), ec);
  }

  if (std::error_code ec; data_acceptor.bind(tcp::endpoint(tcp::v4(), GBA_DATA_PORT), ec) ||
                          data_acceptor.listen(tcp::socket::max_listen_connections, ec))
  {
    ERROR_LOG_FMT(SERIALINTERFACE, "Unable to bind TCP port {}: {}", GBA_DATA_PORT, ec.message());
    return;
  }

  if (std::error_code ec; clock_acceptor.bind(tcp::endpoint(tcp::v4(), GBA_CLOCK_PORT), ec) ||
                          clock_acceptor.listen(asio::socket_base::max_listen_connections, ec))
  {
    ERROR_LOG_FMT(SERIALINTERFACE, "Unable to bind TCP port {}: {}", GBA_CLOCK_PORT, ec.message());
    return;
  }

  const auto enqueue_accepted_sockets =
      [](tcp::acceptor& acceptor, Common::SPSCQueue<tcp::socket>& queue) -> asio::awaitable<void> {
    while (true)
    {
      auto [ec, socket] = co_await acceptor.async_accept(asio::as_tuple);
      if (ec)
        break;

      if (socket.set_option(tcp::no_delay(true), ec))
        ERROR_LOG_FMT(SERIALINTERFACE, "Unable to set socket no_delay: {}", ec.message());

      queue.Emplace(std::move(socket));
    }
  };

  asio::co_spawn(m_io, enqueue_accepted_sockets(data_acceptor, m_waiting_socks), asio::detached);
  asio::co_spawn(m_io, enqueue_accepted_sockets(clock_acceptor, m_waiting_clocks), asio::detached);

  std::stop_callback cb{stoken, [&] { m_io.stop(); }};

  // Keep the io_context running even if our acceptor tasks were to error out somehow.
  // Otherwise outside initiated async tasks would never finish.
  const auto work_guard = asio::make_work_guard(m_io);

  m_io.run();
}

class GBASockServer final
{
public:
  bool Connect();
  bool IsConnected() const;

  void ClockSync(Core::System& system);
  void Send(const u8* si_buffer);
  int Receive(u8* si_buffer, u8 bytes);
  void Flush();

private:
  void Disconnect();

  std::shared_ptr<GBAConnectionHandler> m_connection_handler{GBAConnectionHandler::GetInstance()};

  std::optional<tcp::socket> m_client;
  std::optional<tcp::socket> m_clock_sync;

  u64 m_last_time_slice = 0;
  bool m_booted = false;
};

void GBASockServer::Disconnect()
{
  m_client.reset();
  m_clock_sync.reset();

  m_last_time_slice = 0;
  m_booted = false;
}

void GBASockServer::ClockSync(Core::System& system)
{
  if (!m_clock_sync)
  {
    m_clock_sync = m_connection_handler->GetNextClock();
    if (!m_clock_sync)
      return;
  }

  const auto& core_timing = system.GetCoreTiming();

  u32 time_slice = 0;

  if (m_last_time_slice == 0)
  {
    m_last_time_slice = core_timing.GetTicks();
    time_slice = (u32)(system.GetSystemTimers().GetTicksPerSecond() / 60);
  }
  else
  {
    time_slice = (u32)(core_timing.GetTicks() - m_last_time_slice);
  }

  time_slice = (u32)((u64)time_slice * 16777216 / system.GetSystemTimers().GetTicksPerSecond());
  m_last_time_slice = core_timing.GetTicks();

  std::error_code ec;
  asio::write(*m_clock_sync, Common::AsU8Span(Common::BigEndianValue{time_slice}), ec);
  if (ec)
    m_clock_sync.reset();
}

bool GBASockServer::Connect()
{
  if (IsConnected())
    return true;

  if (auto new_client = m_connection_handler->GetNextSock())
  {
    if (std::error_code ec; new_client->non_blocking(true, ec))
    {
      ERROR_LOG_FMT(SERIALINTERFACE, "Unable to set socket non_blocking: {}", ec.message());
      return false;
    }

    m_client = std::move(*new_client);
  }

  return IsConnected();
}

bool GBASockServer::IsConnected() const
{
  return m_client.has_value();
}

void GBASockServer::Send(const u8* si_buffer)
{
  using SerialInterface::EBufferCommands;

  if (!Connect())
    return;

  const auto cmd = static_cast<EBufferCommands>(si_buffer[0]);
  const std::size_t byte_count = (cmd == EBufferCommands::CMD_WRITE_GBA) ? SEND_MAX_SIZE : 1uz;

  std::error_code ec;
  asio::write(*m_client, asio::buffer(si_buffer, byte_count), ec);
  if (ec)
    Disconnect();
}

int GBASockServer::Receive(u8* si_buffer, u8 bytes)
{
  if (!IsConnected())
    return 0;

  if (m_booted)
  {
    constexpr auto recv_timeout = std::chrono::milliseconds{1000};
    m_client->async_wait(tcp::socket::wait_read, asio::cancel_after(recv_timeout, asio::use_future))
        .wait();
  }

  std::error_code ec;
  const size_t num_received = m_client->read_some(asio::buffer(si_buffer, bytes), ec);
  if (num_received == 0)
  {
    if (ec == asio::error::would_block)
    {
      m_booted = false;
      return 0;
    }

    Disconnect();
    return 0;
  }
  m_booted = true;

  return static_cast<int>(num_received);
}

void GBASockServer::Flush()
{
  if (!IsConnected())
    return;

  std::array<u8, RECV_MAX_SIZE> bytes{};
  for (std::error_code ec; m_client->read_some(asio::buffer(bytes), ec) != 0;)
  {
  }
}

}  // namespace

namespace SerialInterface
{

class CSIDevice_GBA final : public ISIDevice
{
public:
  CSIDevice_GBA(Core::System& system, SIDevices device, int device_number);

  int RunBuffer(u8* buffer, int request_length) override;

  int TransferInterval() override
  {
    return SIDevice_GetGBATransferTime(m_system.GetSystemTimers(), m_last_cmd);
  }

  DataResponse GetData(u32& hi, u32& low) override { return DataResponse::NoData; }

  void SendCommand(u32 command, u8 poll) override {}

private:
  enum class NextAction
  {
    SendCommand,
    WaitTransferTime,
    ReceiveResponse
  };

  GBASockServer m_sock_server;
  NextAction m_next_action = NextAction::SendCommand;
  EBufferCommands m_last_cmd = EBufferCommands::CMD_STATUS;
  u64 m_timestamp_sent = 0;
};

CSIDevice_GBA::CSIDevice_GBA(Core::System& system, SIDevices device, int device_number)
    : ISIDevice(system, device, device_number)
{
}

int CSIDevice_GBA::RunBuffer(u8* buffer, int request_length)
{
  switch (m_next_action)
  {
  case NextAction::SendCommand:
  {
    m_sock_server.ClockSync(m_system);
    if (m_sock_server.Connect())
    {
#ifdef _DEBUG
      NOTICE_LOG_FMT(SERIALINTERFACE, "{} cmd {:02x} [> {:02x}{:02x}{:02x}{:02x}]", m_device_number,
                     buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
#endif
      m_sock_server.Flush();  // Clear out any replies we might have timed out waiting for
      m_sock_server.Send(buffer);
    }
    else
    {
      return -1;
    }

    m_last_cmd = static_cast<EBufferCommands>(buffer[0]);
    m_timestamp_sent = m_system.GetCoreTiming().GetTicks();
    m_next_action = NextAction::WaitTransferTime;
    return 0;
  }

  case NextAction::WaitTransferTime:
  {
    const int elapsed_time =
        static_cast<int>(m_system.GetCoreTiming().GetTicks() - m_timestamp_sent);
    // Tell SI to ask again after TransferInterval() cycles
    if (SIDevice_GetGBATransferTime(m_system.GetSystemTimers(), m_last_cmd) > elapsed_time)
      return 0;
    m_next_action = NextAction::ReceiveResponse;
    [[fallthrough]];
  }

  case NextAction::ReceiveResponse:
  {
    u8 bytes = 1;
    switch (m_last_cmd)
    {
    case EBufferCommands::CMD_RESET:
    case EBufferCommands::CMD_STATUS:
      bytes = 3;
      break;
    case EBufferCommands::CMD_READ_GBA:
      bytes = 5;
      break;
    default:
      break;
    }
    const int num_data_received = m_sock_server.Receive(buffer, bytes);

    m_next_action = NextAction::SendCommand;
    if (num_data_received == 0)
      return -1;
#ifdef _DEBUG
    const Common::Log::LogLevel log_level =
        (m_last_cmd == EBufferCommands::CMD_STATUS || m_last_cmd == EBufferCommands::CMD_RESET) ?
            Common::Log::LogLevel::LERROR :
            Common::Log::LogLevel::LWARNING;
    GENERIC_LOG_FMT(Common::Log::LogType::SERIALINTERFACE, log_level,
                    "{}                              [< {:02x}{:02x}{:02x}{:02x}{:02x}] ({})",
                    m_device_number, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4],
                    num_data_received);
#endif
    return num_data_received;
  }
  }

  // This should never happen, but appease MSVC which thinks it might.
  ERROR_LOG_FMT(SERIALINTERFACE, "Unknown state {}\n", static_cast<int>(m_next_action));
  return 0;
}

std::unique_ptr<ISIDevice> CreateSIDevice_GBA(Core::System& system, SIDevices device,
                                              int device_number)
{
  return std::make_unique<CSIDevice_GBA>(system, device, device_number);
}

}  // namespace SerialInterface
