// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/EXI/EXI_DeviceGecko.h"

#include <span>
#include <stop_token>
#include <thread>

#include <asio.hpp>
#include <asio/experimental/parallel_group.hpp>
#include <fmt/format.h>

#include "Common/AsioUtil.h"  // IWYU pragma: keep
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/SPSCQueue.h"
#include "Common/Thread.h"
#include "Core/Core.h"

namespace
{
using asio::ip::tcp;

class CEXIGecko : public ExpansionInterface::IEXIDevice
{
public:
  using IEXIDevice::IEXIDevice;

  bool IsPresent() const override { return true; }
  void ImmReadWrite(u32& _uData, u32 _uSize) override;

private:
  static constexpr auto FIFO_SIZE = 1024uz;

  asio::awaitable<void> ServerRunLoop();
  asio::awaitable<void> SendLoop(tcp::socket& socket);
  asio::awaitable<void> ReceiveLoop(tcp::socket& socket);

  [[nodiscard]] bool WriteByte(u8 byte);
  [[nodiscard]] bool ReadByte(u8* byte);

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

  static constexpr u32 IDENT = 0x04700000;

  Common::SPSCQueue<u8> m_tx_queue;
  Common::SPSCQueue<u8> m_rx_queue;

  asio::io_context m_io;

  asio::steady_timer m_tx_wait_timer{m_io};
  asio::steady_timer m_rx_wait_timer{m_io};

  std::jthread m_thread{[this](std::stop_token stoken) {
    Common::SetCurrentThreadName("USBGecko");
    asio::co_spawn(m_io, ServerRunLoop(), asio::detached);
    std::stop_callback cb{stoken, [this] { m_io.stop(); }};
    m_io.run();
  }};
};

asio::awaitable<void> CEXIGecko::ServerRunLoop()
{
  tcp::acceptor server{m_io};

  constexpr u16 initial_port = 0xd6ec;  // "dolphin gecko"
  constexpr u16 port_retries = 10;
  constexpr u16 last_port = initial_port + port_retries;

  for (u16 server_port = initial_port; server_port <= last_port; ++server_port)
  {
    tcp::acceptor acceptor{m_io};

    std::error_code ec;
    if (!acceptor.open(tcp::v4(), ec) &&
        !acceptor.bind(tcp::endpoint{tcp::v4(), server_port}, ec) && !acceptor.listen(1, ec))
    {
      Core::DisplayMessage(fmt::format("USBGecko: Listening on TCP port {}", server_port), 5000);
      server = std::move(acceptor);
      break;
    }

    WARN_LOG_FMT(EXPANSIONINTERFACE, "USBGecko: Unable to bind TCP port {}: {}", server_port,
                 ec.message());
  }

  if (!server.is_open())
  {
    const std::string message =
        fmt::format("USBGecko: Failed to bind to any port in range {}-{}", initial_port, last_port);
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "{}", message);
    Core::DisplayMessage(message, 5000);
    co_return;
  }

  while (true)
  {
    auto [ec, socket] = co_await server.async_accept(asio::as_tuple);
    if (ec)
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "USBGecko: Accept failed: {}", ec.message());
      co_return;
    }

    if (socket.set_option(tcp::no_delay(true), ec))
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "Unable to set socket no_delay: {}", ec.message());

    Core::DisplayMessage("USBGecko: Client connected", 5000);

    // This whole thing could be one line if this language had exceptions. ;)
    // co_await (SendLoop(socket) && ReceiveLoop(socket));
    co_await asio::experimental::make_parallel_group(
        asio::co_spawn(m_io, SendLoop(socket), asio::as_tuple),
        asio::co_spawn(m_io, ReceiveLoop(socket), asio::as_tuple))
        .async_wait(asio::experimental::wait_for_all(), asio::as_tuple);

    Core::DisplayMessage("USBGecko: Client disconnected", 5000);
  }
}

asio::awaitable<void> CEXIGecko::SendLoop(tcp::socket& socket)
{
  m_tx_queue.Clear();

  std::array<u8, FIFO_SIZE> buffer{};
  while (socket.is_open())
  {
    const std::size_t send_count = std::min(buffer.size(), m_tx_queue.Size());
    if (send_count == 0)
    {
      // TX FIFO is empty. Wait for some bytes to be written.
      m_tx_wait_timer.expires_after(std::chrono::milliseconds{1});
      co_await m_tx_wait_timer.async_wait(asio::as_tuple);
      continue;
    }

    for (auto i = 0uz; i != send_count; ++i)
      m_tx_queue.Pop(buffer[i]);

    auto [ec, count] =
        co_await socket.async_send(asio::buffer(buffer.data(), send_count), asio::as_tuple);
    if (ec)
    {
      (void)socket.close(ec);
      break;
    }
  }
}

asio::awaitable<void> CEXIGecko::ReceiveLoop(tcp::socket& socket)
{
  std::array<u8, FIFO_SIZE> buffer{};
  while (socket.is_open())
  {
    const std::size_t recv_count = FIFO_SIZE - std::min(m_rx_queue.Size(), FIFO_SIZE);
    if (recv_count == 0)
    {
      // RX FIFO is full. Wait for some bytes to be read.
      m_rx_wait_timer.expires_after(std::chrono::milliseconds{1});
      co_await m_rx_wait_timer.async_wait(asio::as_tuple);
      continue;
    }

    auto [ec, count] =
        co_await socket.async_read_some(asio::buffer(buffer.data(), recv_count), asio::as_tuple);
    if (ec)
    {
      (void)socket.close(ec);
      break;
    }

    for (u8 b : std::span{buffer}.first(count))
      m_rx_queue.Emplace(b);
  }
}

bool CEXIGecko::WriteByte(u8 byte)
{
  if (m_tx_queue.Size() >= FIFO_SIZE)
    return false;

  m_tx_queue.Emplace(byte);
  if (m_tx_queue.Size() == FIFO_SIZE * 3 / 4)
    asio::defer(m_io, [this] { m_tx_wait_timer.cancel(); });
  return true;
}

bool CEXIGecko::ReadByte(u8* byte)
{
  if (!m_rx_queue.Pop(*byte))
    return false;

  if (m_rx_queue.Size() == FIFO_SIZE * 1 / 4)
    asio::defer(m_io, [this] { m_rx_wait_timer.cancel(); });
  return true;
}

void CEXIGecko::ImmReadWrite(u32& _uData, u32 _uSize)
{
  // We don't really care about _uSize
  (void)_uSize;

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
    _uData = IDENT;
    break;

  // PC -> Gecko
  // |= 0x08000000 if successful
  case CMD_RECV:
  {
    u8 byte{};
    if (ReadByte(&byte))
      _uData = 0x08000000 | (byte << 16u);
    break;
  }

  // Gecko -> PC
  // |= 0x04000000 if successful
  case CMD_SEND:
  {
    const u8 byte = _uData >> 20;
    _uData = WriteByte(byte) ? 0x04000000 : 0;
    break;
  }

  // Check if ok for Gecko -> PC, or FIFO full
  // |= 0x04000000 if FIFO is not full
  case CMD_CHK_TX:
  {
    _uData = (m_tx_queue.Size() < FIFO_SIZE) ? 0x04000000 : 0;
    break;
  }

  // Check if data in FIFO for PC -> Gecko, or FIFO empty
  // |= 0x04000000 if data in recv FIFO
  case CMD_CHK_RX:
  {
    _uData = (!m_rx_queue.Empty()) ? 0x04000000 : 0;
    break;
  }

  default:
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "Unknown USBGecko command {:x}", _uData);
    break;
  }
}
}  // namespace

namespace ExpansionInterface
{
std::unique_ptr<IEXIDevice> CreateEXIGecko(Core::System& system)
{
  return std::make_unique<CEXIGecko>(system);
}
}  // namespace ExpansionInterface
