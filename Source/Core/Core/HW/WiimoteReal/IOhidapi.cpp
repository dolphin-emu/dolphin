// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteReal/IOhidapi.h"

#include <algorithm>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"

using namespace WiimoteCommon;
using namespace WiimoteReal;

static bool IsDeviceUsable(const std::string& device_path)
{
  hid_device* handle = hid_open_path(device_path.c_str());
  if (handle == nullptr)
  {
    ERROR_LOG_FMT(WIIMOTE,
                  "Could not connect to Wii Remote at \"{}\". "
                  "Do you have permission to access the device?",
                  device_path);
    return false;
  }
  // Some third-party adapters (DolphinBar) always expose all four Wii Remotes as HIDs
  // even when they are not connected, which causes an endless error loop when we try to use them.
  // Try to write a report to the device to see if this Wii Remote is really usable.
  static const u8 report[] = {WR_SET_REPORT | BT_OUTPUT, u8(OutputReportID::RequestStatus), 0};
  const int result = hid_write(handle, report, sizeof(report));
  // The DolphinBar uses EPIPE to signal the absence of a Wii Remote connected to this HID.
  if (result == -1 && errno != EPIPE)
    ERROR_LOG_FMT(WIIMOTE, "Couldn't write to Wii Remote at \"{}\".", device_path);

  hid_close(handle);
  return result != -1;
}

namespace WiimoteReal
{
WiimoteScannerHidapi::WiimoteScannerHidapi()
{
  int ret = hid_init();
  ASSERT_MSG(WIIMOTE, ret == 0, "Couldn't initialise hidapi.");
}

WiimoteScannerHidapi::~WiimoteScannerHidapi()
{
  if (hid_exit() == -1)
    ERROR_LOG_FMT(WIIMOTE, "Failed to clean up hidapi.");
}

bool WiimoteScannerHidapi::IsReady() const
{
  return true;
}

void WiimoteScannerHidapi::FindWiimotes(std::vector<Wiimote*>& wiimotes, Wiimote*& board)
{
  hid_device_info* list = hid_enumerate(0x0, 0x0);
  for (hid_device_info* device = list; device; device = device->next)
  {
    const std::string name = device->product_string ? WStringToUTF8(device->product_string) : "";
    const bool is_wiimote =
        IsValidDeviceName(name) || (device->vendor_id == 0x057e &&
                                    (device->product_id == 0x0306 || device->product_id == 0x0330));
    if (!is_wiimote || !IsNewWiimote(device->path) || !IsDeviceUsable(device->path))
      continue;

    auto* wiimote = new WiimoteHidapi(device->path);
    const bool is_balance_board = IsBalanceBoardName(name) || wiimote->IsBalanceBoard();
    if (is_balance_board)
      board = wiimote;
    else
      wiimotes.push_back(wiimote);

    NOTICE_LOG_FMT(WIIMOTE, "Found {} at {}: {} {} ({:04x}:{:04x})",
                   is_balance_board ? "balance board" : "Wiimote", device->path,
                   WStringToUTF8(device->manufacturer_string),
                   WStringToUTF8(device->product_string), device->vendor_id, device->product_id);
  }
  hid_free_enumeration(list);
}

WiimoteHidapi::WiimoteHidapi(const std::string& device_path) : m_device_path(device_path)
{
}

WiimoteHidapi::~WiimoteHidapi()
{
  Shutdown();
}

bool WiimoteHidapi::ConnectInternal()
{
  if (m_handle != nullptr)
    return true;

  m_handle = hid_open_path(m_device_path.c_str());
  if (m_handle == nullptr)
  {
    ERROR_LOG_FMT(WIIMOTE,
                  "Could not connect to Wii Remote at \"{}\". "
                  "Do you have permission to access the device?",
                  m_device_path);
  }
  return m_handle != nullptr;
}

void WiimoteHidapi::DisconnectInternal()
{
  hid_close(m_handle);
  m_handle = nullptr;
}

bool WiimoteHidapi::IsConnected() const
{
  return m_handle != nullptr;
}

int WiimoteHidapi::IORead(u8* buf)
{
  int timeout = 200;  // ms
  int result = hid_read_timeout(m_handle, buf + 1, MAX_PAYLOAD - 1, timeout);
  // TODO: If and once we use hidapi across plaforms, change our internal API to clean up this mess.
  if (result == -1)
  {
    ERROR_LOG_FMT(WIIMOTE, "Failed to read from {}.", m_device_path);
    return 0;  // error
  }
  if (result == 0)
  {
    return -1;  // didn't read packet
  }
  buf[0] = WR_SET_REPORT | BT_INPUT;
  return result + 1;  // number of bytes read
}

int WiimoteHidapi::IOWrite(const u8* buf, size_t len)
{
  DEBUG_ASSERT(buf[0] == (WR_SET_REPORT | BT_OUTPUT));
  int result = hid_write(m_handle, buf + 1, len - 1);
  if (result == -1)
  {
    ERROR_LOG_FMT(WIIMOTE, "Failed to write to {}.", m_device_path);
    return 0;
  }
  return (result == 0) ? 1 : result;
}
};  // namespace WiimoteReal
