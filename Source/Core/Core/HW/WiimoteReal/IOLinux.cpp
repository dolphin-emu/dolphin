// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteReal/IOLinux.h"

#include <ranges>
#include <vector>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/signalfd.h>

#include <unistd.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"
#include "Common/ScopeGuard.h"
#include "Common/UnixUtil.h"
#include "Core/Config/MainSettings.h"

namespace WiimoteReal
{
constexpr u16 L2CAP_PSM_HID_CNTL = 0x0011;
constexpr u16 L2CAP_PSM_HID_INTR = 0x0013;

static auto GetAutoConnectAddresses()
{
  std::vector<std::unique_ptr<Wiimote>> wii_remotes;

  std::string entries = Config::Get(Config::MAIN_WIIMOTE_AUTO_CONNECT_ADDRESSES);
  for (auto& bt_address_str : SplitString(entries, ','))
  {
    const auto bt_addr = Common::StringToBluetoothAddress(bt_address_str);
    if (!bt_addr.has_value())
    {
      WARN_LOG_FMT(WIIMOTE, "Bad Auto Connect Bluetooth Address: {}", bt_address_str);
      continue;
    }

    Common::ToLower(&bt_address_str);
    if (!IsNewWiimote(bt_address_str))
      continue;

    wii_remotes.emplace_back(std::make_unique<WiimoteLinux>(*bt_addr));
    NOTICE_LOG_FMT(WIIMOTE, "Added Wiimote with fixed address ({}).", bt_address_str);
  }

  return wii_remotes;
}

WiimoteScannerLinux::WiimoteScannerLinux()
{
  Open();
}

bool WiimoteScannerLinux::Open()
{
  if (IsReady())
  {
    // Verify that the device socket is still valid (checking for POLLERR).
    pollfd pfd{.fd = m_device_sock};
    if (UnixUtil::RetryOnEINTR(poll, &pfd, 1, 0) >= 0 && pfd.revents == 0)
      return true;

    // This happens if the Bluetooth adapter was unplugged between scans.
    WARN_LOG_FMT(WIIMOTE, "Existing Bluetooth socket was invalid. Attempting to reopen.");
    Close();
  }

  // Get the id of the first Bluetooth device.
  m_device_id = hci_get_route(nullptr);
  if (m_device_id < 0)
  {
    NOTICE_LOG_FMT(WIIMOTE, "Bluetooth not found. hci_get_route: {}", Common::LastStrerrorString());
    return false;
  }

  // Create a socket to the device
  m_device_sock = hci_open_dev(m_device_id);
  if (m_device_sock < 0)
  {
    ERROR_LOG_FMT(WIIMOTE, "Unable to open Bluetooth. hci_open_dev: {}",
                  Common::LastStrerrorString());
    return false;
  }

  m_is_device_open.store(true, std::memory_order_relaxed);
  return true;
}

WiimoteScannerLinux::~WiimoteScannerLinux()
{
  Close();
}

void WiimoteScannerLinux::Close()
{
  if (!IsReady())
    return;

  m_is_device_open.store(false, std::memory_order_relaxed);

  close(std::exchange(m_device_sock, -1));
  m_device_id = -1;
}

bool WiimoteScannerLinux::IsReady() const
{
  return m_is_device_open.load(std::memory_order_relaxed);
}

struct InquiryRequest : hci_inquiry_req
{
  static constexpr int MAX_INFOS = 255;
  std::array<inquiry_info, MAX_INFOS> scan_infos;
};
static_assert(sizeof(InquiryRequest) ==
              sizeof(hci_inquiry_req) + sizeof(inquiry_info) * InquiryRequest::MAX_INFOS);

// Returns 0 on success or some error number on failure.
static int HciInquiry(int device_socket, InquiryRequest* request)
{
  const auto done_event = UnixUtil::CreateEventFD(0, 0);
  Common::ScopeGuard close_guard([&] { close(done_event); });

  int hci_inquiry_errorno = 0;

  // Unplugging a BT adapter causes `hci_inquiry` to block forever (inside `ioctl`).
  // Fortunately it does produce a signal on the socket so we can poll for that.
  // Performing the inquiry on thread lets us interrupt `ioctl` if the socket signals.

  // Using `pthread_cancel` with `std::thread` isn't technically correct so we use `pthread_create`.
  UnixUtil::PThreadWrapper hci_inquiry_thread{[&] {
    // We're manually doing the `ioctl` because `hci_inquiry` isn't pthread_cancel-safe.
    // It uses `malloc` and whatnot.
    const int ret = ioctl(device_socket, HCIINQUIRY, reinterpret_cast<unsigned long>(request));
    if (ret < 0)
      hci_inquiry_errorno = errno;

    // Signal doneness to `poll`.
    u64 val = 1;
    if (write(done_event, &val, sizeof(val)) != sizeof(val))
      ERROR_LOG_FMT(WIIMOTE, "failed to write to eventfd: {}", Common::LastStrerrorString());
  }};
  Common::ScopeGuard join_guard([&] { pthread_join(hci_inquiry_thread.handle, nullptr); });

  // Wait for the above thread or some socket signal.
  std::array<pollfd, 2> pollfds{
      pollfd{.fd = device_socket},
      pollfd{.fd = done_event, .events = POLLIN},
  };
  UnixUtil::RetryOnEINTR(poll, pollfds.data(), pollfds.size(), -1);

  if (pollfds[0].revents != 0)
  {
    ERROR_LOG_FMT(WIIMOTE, "HciInquiry device socket had error. Cancelling thread.");
    // I am not entirely sure if this is foolproof, depending on where `ioctl` is internally?
    // It's worked every time in testing though, and it's better than *always* freezing.
    pthread_cancel(hci_inquiry_thread.handle);
    return ENODEV;
  }

  return hci_inquiry_errorno;
}

auto WiimoteScannerLinux::FindNewWiimotes() -> FindResults
{
  FindResults results;

  if (!Open())
    return results;

  results.wii_remotes = GetAutoConnectAddresses();

  InquiryRequest request{};
  request.dev_id = m_device_id;
  request.flags = IREQ_CACHE_FLUSH;
  request.length = BLUETOOTH_INQUIRY_LENGTH;
  request.num_rsp = InquiryRequest::MAX_INFOS;
  // Use Limited Dedicated Inquiry Access Code (LIAC) like the Wii does.
  // Third-party Wiimotes cannot be discovered without it.
  std::ranges::copy(std::to_array({0x00, 0x8b, 0x9e}), request.lap);

  const int hci_inquiry_result = HciInquiry(m_device_sock, &request);
  switch (hci_inquiry_result)
  {
  case 0:
    break;
  case ENODEV:
    Close();
    [[fallthrough]];
  default:
    ERROR_LOG_FMT(WIIMOTE, "Error searching for Bluetooth devices: {}",
                  Common::StrerrorString(hci_inquiry_result));
    return results;
  }

  DEBUG_LOG_FMT(WIIMOTE, "Found {} Bluetooth device(s).", request.num_rsp);

  for (auto& scan_info : request.scan_infos | std::ranges::views::take(request.num_rsp))
  {
    const auto bdaddr_str =
        BluetoothAddressToString(std::bit_cast<Common::BluetoothAddress>(scan_info.bdaddr));

    // Did AddAutoConnectAddresses already add this remote?
    const auto eq_this_bdaddr = [&](auto& wm) { return wm->GetId() == bdaddr_str; };
    if (std::ranges::any_of(results.wii_remotes, eq_this_bdaddr))
      continue;

    if (!IsNewWiimote(bdaddr_str))
    {
      WARN_LOG_FMT(WIIMOTE, "Discovered already connected device: {}", bdaddr_str);
      continue;
    }

    // The Wii can actually connect remotes with a 10 second name response time.
    // We won't wait quite so long since we're doing this in a blocking manner right now.
    // Note that waiting just 1 second can be problematic sometimes.
    const int read_name_timeout_ms = 3000;

    // BT names are a maximum of 248 bytes.
    char name[255]{};
    if (hci_read_remote_name_with_clock_offset(m_device_sock, &scan_info.bdaddr,
                                               scan_info.pscan_rep_mode, scan_info.clock_offset,
                                               sizeof(name), name, read_name_timeout_ms) < 0)
    {
      ERROR_LOG_FMT(WIIMOTE, "Bluetooth read remote name failed. hci_read_remote_name: {}",
                    Common::LastStrerrorString());
      continue;
    }

    INFO_LOG_FMT(WIIMOTE, "Found bluetooth device with name: {}", name);

    if (!IsValidDeviceName(name))
      continue;

    // Found a new device
    auto wm =
        std::make_unique<WiimoteLinux>(std::bit_cast<Common::BluetoothAddress>(scan_info.bdaddr));
    if (IsBalanceBoardName(name))
    {
      results.balance_boards.emplace_back(std::move(wm));
      NOTICE_LOG_FMT(WIIMOTE, "Found balance board ({}).", bdaddr_str);
    }
    else
    {
      results.wii_remotes.emplace_back(std::move(wm));
      NOTICE_LOG_FMT(WIIMOTE, "Found Wiimote ({}).", bdaddr_str);
    }
  }

  return results;
}

void WiimoteScannerLinux::Update()
{  // Nothing needed on Linux.
}

void WiimoteScannerLinux::RequestStopSearching()
{  // Nothing needed on Linux.
}

WiimoteLinux::WiimoteLinux(Common::BluetoothAddress bdaddr)
    : m_bdaddr{bdaddr}, m_wakeup_fd{UnixUtil::CreateEventFD(0, 0)}
{
  m_really_disconnect = true;
}

WiimoteLinux::~WiimoteLinux()
{
  Shutdown();
  close(m_wakeup_fd);
}

std::string WiimoteLinux::GetId() const
{
  return BluetoothAddressToString(m_bdaddr);
}

// Connect to a Wiimote with a known address.
bool WiimoteLinux::ConnectInternal()
{
  sockaddr_l2 addr{
      .l2_family = AF_BLUETOOTH,
      .l2_bdaddr = std::bit_cast<bdaddr_t>(m_bdaddr),
      .l2_cid = 0,
  };

  const auto open_channel = [&](u16 l2_psm) {
    addr.l2_psm = htobs(l2_psm);

    constexpr int total_tries = 3;
    for (int i = 0; i != total_tries; ++i)
    {
      const int descriptor = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
      if (descriptor == -1)
      {
        WARN_LOG_FMT(WIIMOTE, "Failed to create L2CAP socket: {}", Common::LastStrerrorString());
        return -1;
      }

      if (connect(descriptor, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0)
        return descriptor;

      // If connecting fails sleep and try again.
      WARN_LOG_FMT(WIIMOTE, "Failed to connect L2CAP PSM({}) socket: {}", l2_psm,
                   Common::LastStrerrorString());
      // A socket state is unspecified after connect() fails, so close and recreate it.
      close(descriptor);
      std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }

    return -1;
  };

  m_cmd_sock = open_channel(L2CAP_PSM_HID_CNTL);
  if (m_cmd_sock == -1)
    return false;

  m_int_sock = open_channel(L2CAP_PSM_HID_INTR);
  if (m_int_sock == -1)
  {
    close(std::exchange(m_cmd_sock, -1));
    return false;
  }

  return true;
}

void WiimoteLinux::DisconnectInternal()
{
  close(std::exchange(m_cmd_sock, -1));
  close(std::exchange(m_int_sock, -1));
}

bool WiimoteLinux::IsConnected() const
{
  return m_cmd_sock != -1;  // && int_sock != -1;
}

void WiimoteLinux::IOWakeup()
{
  u64 counter = 1;
  if (write(m_wakeup_fd, &counter, sizeof(counter)) != sizeof(counter))
  {
    ERROR_LOG_FMT(WIIMOTE, "failed to write to wakeup eventfd: {}", Common::LastStrerrorString());
  }
}

// positive = read packet
// negative = didn't read packet
// zero = error
int WiimoteLinux::IORead(u8* buf)
{
  std::array<pollfd, 2> pollfds{
      pollfd{.fd = m_wakeup_fd, .events = POLLIN},
      pollfd{.fd = m_int_sock, .events = POLLIN},
  };
  UnixUtil::RetryOnEINTR(poll, pollfds.data(), pollfds.size(), -1);

  // Handle IOWakeup.
  if (pollfds[0].revents != 0)
  {
    DEBUG_LOG_FMT(WIIMOTE, "IOWakeup");
    u64 counter{};
    if (read(m_wakeup_fd, &counter, sizeof(counter)) != sizeof(counter))
    {
      ERROR_LOG_FMT(WIIMOTE, "Failed to read from wakeup eventfd: {}",
                    Common::LastStrerrorString());
      return 0;
    }
    return -1;
  }

  // Handle event on interrupt channel.
  auto result = int(read(m_int_sock, buf, MAX_PAYLOAD));
  if (result == -1)
  {
    ERROR_LOG_FMT(WIIMOTE, "Wiimote {} read failed: {}", m_index + 1, Common::LastStrerrorString());
    result = 0;
  }

  return result;
}

int WiimoteLinux::IOWrite(u8 const* buf, size_t len)
{
  auto result = int(write(m_int_sock, buf, int(len)));
  if (result == -1)
  {
    ERROR_LOG_FMT(WIIMOTE, "Wiimote {} write failed: {}", m_index + 1,
                  Common::LastStrerrorString());
  }

  return result;
}

}  // namespace WiimoteReal
