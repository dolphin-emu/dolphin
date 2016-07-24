// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/HW/WiimoteEmu/WiimoteHid.h"
#include "Core/HW/WiimoteReal/IOhidapi.h"

// This is used to store connected Wiimotes' paths, so we don't connect
// more than once to the same device.
static std::vector<std::string> s_known_paths;
static bool IsNewDevice(const std::string& device_path)
{
  return std::find(s_known_paths.begin(), s_known_paths.end(), device_path) == s_known_paths.end();
}
static void ForgetDevicePath(const std::string& device_path)
{
  s_known_paths.erase(std::remove(s_known_paths.begin(), s_known_paths.end(), device_path),
                      s_known_paths.end());
}

static bool IsDeviceUsable(const std::string& device_path)
{
  hid_device* handle = hid_open_path(device_path.c_str());
  if (handle == nullptr)
  {
    ERROR_LOG(WIIMOTE, "Could not connect to Wiimote at \"%s\". "
                       "Do you have permission to access the device?",
              device_path.c_str());
    return false;
  }
  // Some third-party adapters (DolphinBar) always expose all four Wiimotes as HIDs
  // even when they are not connected, which causes an endless error loop when we try to use them.
  // Try to write a report to the device to see if this Wiimote is really usable.
  u8 static const report[] = {WM_SET_REPORT | WM_BT_OUTPUT, WM_REQUEST_STATUS, 0};
  const int result = hid_write(handle, report, sizeof(report));
  // The DolphinBar uses EPIPE to signal the absence of a Wiimote connected to this HID.
  if (result == -1 && errno != EPIPE)
    ERROR_LOG(WIIMOTE, "Couldn't write to Wiimote at \"%s\".", device_path.c_str());

  hid_close(handle);
  return result != -1;
}

namespace WiimoteReal
{
WiimoteScannerHidapi::WiimoteScannerHidapi()
{
  int ret = hid_init();
  _assert_msg_(WIIMOTE, ret == 0, "Couldn't initialise hidapi.");
}

WiimoteScannerHidapi::~WiimoteScannerHidapi()
{
  if (hid_exit() == -1)
    ERROR_LOG(WIIMOTE, "Failed to clean up hidapi.");
}

bool WiimoteScannerHidapi::IsReady() const
{
  return true;
}

void WiimoteScannerHidapi::FindWiimotes(std::vector<Wiimote*>& wiimotes, Wiimote*& board)
{
  hid_device_info* list = hid_enumerate(0x0, 0x0);
  hid_device_info* device = list;
  while (device)
  {
    const std::string name = device->product_string ? UTF16ToUTF8(device->product_string) : "";
    const bool is_wiimote =
        IsValidDeviceName(name) || (device->vendor_id == 0x057e &&
                                    (device->product_id == 0x0306 || device->product_id == 0x0330));
    if (!is_wiimote || !IsNewDevice(device->path) || !IsDeviceUsable(device->path))
    {
      device = device->next;
      continue;
    }

    const bool is_balance_board = IsBalanceBoardName(name);
    NOTICE_LOG(WIIMOTE, "Found %s at %s: %ls %ls (%04hx:%04hx)",
               is_balance_board ? "balance board" : "Wiimote", device->path,
               device->manufacturer_string, device->product_string, device->vendor_id,
               device->product_id);

    Wiimote* wiimote = new WiimoteHidapi(device->path);
    s_known_paths.push_back(device->path);
    if (is_balance_board)
      board = wiimote;
    else
      wiimotes.push_back(wiimote);

    device = device->next;
  }
  hid_free_enumeration(list);
}

WiimoteHidapi::WiimoteHidapi(char* device_path) : m_device_path(device_path), m_handle(nullptr)
{
}

WiimoteHidapi::~WiimoteHidapi()
{
  Shutdown();
  ForgetDevicePath(m_device_path);
}

bool WiimoteHidapi::ConnectInternal()
{
  m_handle = hid_open_path(m_device_path.c_str());
  if (m_handle == nullptr)
  {
    ERROR_LOG(WIIMOTE, "Could not connect to Wiimote at \"%s\". "
                       "Do you have permission to access the device?",
              m_device_path.c_str());
    ForgetDevicePath(m_device_path);
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
    ERROR_LOG(WIIMOTE, "Failed to read from %s.", m_device_path.c_str());
    return 0;  // error
  }
  if (result == 0)
  {
    return -1;  // didn't read packet
  }
  buf[0] = WM_SET_REPORT | WM_BT_INPUT;
  return result + 1;  // number of bytes read
}

int WiimoteHidapi::IOWrite(u8 const* buf, size_t len)
{
  _dbg_assert_(WIIMOTE, buf[0] == (WM_SET_REPORT | WM_BT_OUTPUT));
  int result = hid_write(m_handle, buf + 1, len - 1);
  if (result == -1)
  {
    ERROR_LOG(WIIMOTE, "Failed to write to %s.", m_device_path.c_str());
    return 0;
  }
  return (result == 0) ? 1 : result;
}
};  // WiimoteReal
