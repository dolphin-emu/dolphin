// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteReal/IOwmhci.h"

#if defined(__APPLE__) || defined(__linux__)

#include <cerrno>
#include <cstring>
#include <filesystem>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "Common/Logging/Log.h"

namespace WiimoteReal
{
static constexpr const char* WMHCI_SOCKET_DIR = "/tmp/wmhci";

auto WiimoteScannerWmhci::FindAttachedWiimotes() -> FindResults
{
  FindResults results;

  std::error_code err;
  for (const auto& entry : std::filesystem::directory_iterator(WMHCI_SOCKET_DIR, err))
  {
    const std::string path = entry.path().string();
    if (entry.path().extension() != ".sock" || !IsNewWiimote(path))
      continue;

    auto wiimote = std::make_unique<WiimoteWmhci>(path);
    const bool is_balance_board = wiimote->IsBalanceBoard();
    NOTICE_LOG_FMT(WIIMOTE, "Found {} via wmhcid at {}",
                   is_balance_board ? "balance board" : "Wii Remote", path);
    if (is_balance_board)
      results.balance_boards.emplace_back(std::move(wiimote));
    else
      results.wii_remotes.emplace_back(std::move(wiimote));
  }

  return results;
}

WiimoteWmhci::WiimoteWmhci(std::string socket_path) : m_socket_path(std::move(socket_path))
{
  if (pipe(m_wakeup_pipe) == 0)
  {
    fcntl(m_wakeup_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(m_wakeup_pipe[1], F_SETFL, O_NONBLOCK);
  }
}

WiimoteWmhci::~WiimoteWmhci()
{
  Shutdown();
  for (int fd : m_wakeup_pipe)
  {
    if (fd >= 0)
      close(fd);
  }
}

bool WiimoteWmhci::ConnectInternal()
{
  if (m_fd >= 0)
    return true;

  m_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (m_fd < 0)
    return false;

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, m_socket_path.c_str(), sizeof(addr.sun_path) - 1);
  if (connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
  {
    ERROR_LOG_FMT(WIIMOTE, "wmhcid: could not connect to {}: {}", m_socket_path,
                  std::strerror(errno));
    close(m_fd);
    m_fd = -1;
    return false;
  }

#ifdef SO_NOSIGPIPE
  const int one = 1;
  setsockopt(m_fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
  return true;
}

void WiimoteWmhci::DisconnectInternal()
{
  if (m_fd >= 0)
    close(m_fd);
  m_fd = -1;
}

bool WiimoteWmhci::IsConnected() const
{
  return m_fd >= 0;
}

void WiimoteWmhci::IOWakeup()
{
  const u8 byte = 0;
  if (m_wakeup_pipe[1] >= 0)
  {
    [[maybe_unused]] const auto result = write(m_wakeup_pipe[1], &byte, 1);
  }
}

bool WiimoteWmhci::ReadExact(u8* buf, size_t len)
{
  while (len > 0)
  {
    const ssize_t n = read(m_fd, buf, len);
    if (n <= 0)
    {
      if (n < 0 && (errno == EINTR || errno == EAGAIN))
        continue;
      return false;
    }
    buf += n;
    len -= static_cast<size_t>(n);
  }
  return true;
}

int WiimoteWmhci::IORead(u8* buf)
{
  pollfd fds[2] = {{m_fd, POLLIN, 0}, {m_wakeup_pipe[0], POLLIN, 0}};
  const int ready = poll(fds, 2, 200);
  if (ready <= 0)
    return -1;  // no report yet

  if (fds[1].revents & POLLIN)
  {
    u8 drain[16];
    while (read(m_wakeup_pipe[0], drain, sizeof(drain)) > 0)
    {
    }
    if (!(fds[0].revents & POLLIN))
      return -1;
  }

  if (fds[0].revents & (POLLHUP | POLLERR) && !(fds[0].revents & POLLIN))
    return 0;  // wmhcid lost the remote

  u8 len = 0;
  if (!ReadExact(&len, 1) || len == 0 || len > MAX_PAYLOAD)
    return 0;
  if (!ReadExact(buf, len))
    return 0;
  return len;
}

int WiimoteWmhci::IOWrite(const u8* buf, size_t len)
{
  if (len == 0 || len > MAX_PAYLOAD)
    return 0;

  u8 frame[MAX_PAYLOAD + 1];
  frame[0] = static_cast<u8>(len);
  std::memcpy(frame + 1, buf, len);

  size_t off = 0;
  while (off < len + 1)
  {
    const ssize_t n = write(m_fd, frame + off, len + 1 - off);
    if (n <= 0)
    {
      if (n < 0 && errno == EINTR)
        continue;
      ERROR_LOG_FMT(WIIMOTE, "wmhcid: write to {} failed", m_socket_path);
      return 0;
    }
    off += static_cast<size_t>(n);
  }
  return static_cast<int>(len);
}
}  // namespace WiimoteReal

#endif
