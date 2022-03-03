// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteReal/IOLinux.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#include <sys/poll.h>
#include <unistd.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

namespace WiimoteReal
{
constexpr u16 L2CAP_PSM_HID_CNTL = 0x0011;
constexpr u16 L2CAP_PSM_HID_INTR = 0x0013;

WiimoteScannerLinux::WiimoteScannerLinux() : m_device_id(-1), m_device_sock(-1)
{
  // Get the id of the first Bluetooth device.
  m_device_id = hci_get_route(nullptr);
  if (m_device_id < 0)
  {
    NOTICE_LOG_FMT(WIIMOTE, "Bluetooth not found.");
    return;
  }

  // Create a socket to the device
  m_device_sock = hci_open_dev(m_device_id);
  if (m_device_sock < 0)
  {
    ERROR_LOG_FMT(WIIMOTE, "Unable to open Bluetooth.");
    return;
  }
}

WiimoteScannerLinux::~WiimoteScannerLinux()
{
  if (IsReady())
    close(m_device_sock);
}

bool WiimoteScannerLinux::IsReady() const
{
  return m_device_sock > 0;
}

void WiimoteScannerLinux::FindWiimotes(std::vector<Wiimote*>& found_wiimotes, Wiimote*& found_board)
{
  // supposedly 1.28 seconds
  int const wait_len = 1;

  int const max_infos = 255;
  inquiry_info scan_infos[max_infos] = {};
  auto* scan_infos_ptr = scan_infos;
  found_board = nullptr;
  // Use Limited Dedicated Inquiry Access Code (LIAC) to query, since third-party Wiimotes
  // cannot be discovered without it.
  const u8 lap[3] = {0x00, 0x8b, 0x9e};

  // Scan for Bluetooth devices
  int const found_devices =
      hci_inquiry(m_device_id, wait_len, max_infos, lap, &scan_infos_ptr, IREQ_CACHE_FLUSH);
  if (found_devices < 0)
  {
    ERROR_LOG_FMT(WIIMOTE, "Error searching for Bluetooth devices.");
    return;
  }

  DEBUG_LOG_FMT(WIIMOTE, "Found {} Bluetooth device(s).", found_devices);

  // Display discovered devices
  for (int i = 0; i < found_devices; ++i)
  {
    NOTICE_LOG_FMT(WIIMOTE, "found a device...");

    // BT names are a maximum of 248 bytes apparently
    char name[255] = {};
    if (hci_read_remote_name(m_device_sock, &scan_infos[i].bdaddr, sizeof(name), name, 1000) < 0)
    {
      ERROR_LOG_FMT(WIIMOTE, "name request failed");
      continue;
    }

    NOTICE_LOG_FMT(WIIMOTE, "device name {}", name);
    if (!IsValidDeviceName(name))
      continue;

    char bdaddr_str[18] = {};
    ba2str(&scan_infos[i].bdaddr, bdaddr_str);

    if (!IsNewWiimote(bdaddr_str))
      continue;

    // Found a new device
    Wiimote* wm = new WiimoteLinux(scan_infos[i].bdaddr);
    if (IsBalanceBoardName(name))
    {
      found_board = wm;
      NOTICE_LOG_FMT(WIIMOTE, "Found balance board ({}).", bdaddr_str);
    }
    else
    {
      found_wiimotes.push_back(wm);
      NOTICE_LOG_FMT(WIIMOTE, "Found Wiimote ({}).", bdaddr_str);
    }
  }
}

WiimoteLinux::WiimoteLinux(bdaddr_t bdaddr) : m_bdaddr(bdaddr)
{
  m_really_disconnect = true;

  m_cmd_sock = -1;
  m_int_sock = -1;

  int fds[2];
  if (pipe(fds))
  {
    ERROR_LOG_FMT(WIIMOTE, "pipe failed");
    abort();
  }
  m_wakeup_pipe_w = fds[1];
  m_wakeup_pipe_r = fds[0];
}

WiimoteLinux::~WiimoteLinux()
{
  Shutdown();
  close(m_wakeup_pipe_w);
  close(m_wakeup_pipe_r);
}

// Connect to a Wiimote with a known address.
bool WiimoteLinux::ConnectInternal()
{
  sockaddr_l2 addr = {};
  addr.l2_family = AF_BLUETOOTH;
  addr.l2_bdaddr = m_bdaddr;
  addr.l2_cid = 0;

  // Control channel
  addr.l2_psm = htobs(L2CAP_PSM_HID_CNTL);
  if ((m_cmd_sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)))
  {
    int retry = 0;
    while (connect(m_cmd_sock, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
      // If opening channel fails sleep and try again
      if (retry == 3)
      {
        WARN_LOG_FMT(WIIMOTE, "Unable to connect control channel of Wiimote: {}", strerror(errno));
        close(m_cmd_sock);
        m_cmd_sock = -1;
        return false;
      }
      retry++;
      sleep(1);
    }
  }
  else
  {
    WARN_LOG_FMT(WIIMOTE, "Unable to open control socket to Wiimote: {}", strerror(errno));
    return false;
  }

  // Interrupt channel
  addr.l2_psm = htobs(L2CAP_PSM_HID_INTR);
  if ((m_int_sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)))
  {
    int retry = 0;
    while (connect(m_int_sock, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
      // If opening channel fails sleep and try again
      if (retry == 3)
      {
        WARN_LOG_FMT(WIIMOTE, "Unable to connect interrupt channel of Wiimote: {}",
                     strerror(errno));
        close(m_int_sock);
        close(m_cmd_sock);
        m_int_sock = m_cmd_sock = -1;
        return false;
      }
      retry++;
      sleep(1);
    }
  }
  else
  {
    WARN_LOG_FMT(WIIMOTE, "Unable to open interrupt socket to Wiimote: {}", strerror(errno));
    close(m_cmd_sock);
    m_int_sock = m_cmd_sock = -1;
    return false;
  }

  return true;
}

void WiimoteLinux::DisconnectInternal()
{
  close(m_cmd_sock);
  close(m_int_sock);

  m_cmd_sock = -1;
  m_int_sock = -1;
}

bool WiimoteLinux::IsConnected() const
{
  return m_cmd_sock != -1;  // && int_sock != -1;
}

void WiimoteLinux::IOWakeup()
{
  char c = 0;
  if (write(m_wakeup_pipe_w, &c, 1) != 1)
  {
    ERROR_LOG_FMT(WIIMOTE, "Unable to write to wakeup pipe.");
  }
}

// positive = read packet
// negative = didn't read packet
// zero = error
int WiimoteLinux::IORead(u8* buf)
{
  std::array<pollfd, 2> pollfds = {};

  auto& poll_wakeup = pollfds[0];
  poll_wakeup.fd = m_wakeup_pipe_r;
  poll_wakeup.events = POLLIN;

  auto& poll_sock = pollfds[1];
  poll_sock.fd = m_int_sock;
  poll_sock.events = POLLIN;

  if (poll(pollfds.data(), pollfds.size(), -1) == -1)
  {
    ERROR_LOG_FMT(WIIMOTE, "Unable to poll Wiimote {} input socket.", m_index + 1);
    return -1;
  }

  if (poll_wakeup.revents & POLLIN)
  {
    char c;
    if (read(m_wakeup_pipe_r, &c, 1) != 1)
    {
      ERROR_LOG_FMT(WIIMOTE, "Unable to read from wakeup pipe.");
    }
    return -1;
  }

  if (!(poll_sock.revents & POLLIN))
    return -1;

  // Read the pending message into the buffer
  int r = read(m_int_sock, buf, MAX_PAYLOAD);
  if (r == -1)
  {
    // Error reading data
    ERROR_LOG_FMT(WIIMOTE, "Receiving data from Wiimote {}.", m_index + 1);

    if (errno == ENOTCONN)
    {
      // This can happen if the Bluetooth dongle is disconnected
      ERROR_LOG_FMT(WIIMOTE,
                    "Bluetooth appears to be disconnected.  "
                    "Wiimote {} will be disconnected.",
                    m_index + 1);
    }

    r = 0;
  }

  return r;
}

int WiimoteLinux::IOWrite(u8 const* buf, size_t len)
{
  return write(m_int_sock, buf, (int)len);
}

};  // namespace WiimoteReal
