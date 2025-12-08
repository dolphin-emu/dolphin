// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteReal/IOWin.h"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <Windows.h>
#include <BluetoothAPIs.h>
#include <Cfgmgr32.h>
#include <Hidclass.h>
#include <Hidsdi.h>
#include <initguid.h>
// initguid.h must be included before Devpkey.h
#include <Devpkey.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/WindowsDevice.h"

#include "Core/HW/WiimoteCommon/DataReport.h"
#include "Core/HW/WiimoteCommon/WiimoteConstants.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"

#pragma comment(lib, "Bthprops.lib")

namespace WiimoteReal
{
constexpr u8 DEFAULT_INQUIRY_LENGTH = 3;

enum class EnumerationControl : bool
{
  Stop,
  Continue,
};

// Use inquiry_length of 0 to enumerate known devices.
void EnumerateBluetoothWiimotes(u8 inquiry_length, auto&& enumeration_callback)
{
  EnumerateBluetoothDevices(inquiry_length,
                            [&](HANDLE radio_handle, const auto& radio_info, auto* btdi) {
                              // Does it have an RVL- name?
                              const auto name = WStringToUTF8(btdi->szName);
                              if (!IsValidDeviceName(name))
                                return EnumerationControl::Continue;

                              return enumeration_callback(radio_handle, radio_info, btdi);
                            });
}

enum class AuthenticationMethod : bool
{
  OneTwo,
  SyncButton,
};

// BluetoothAuthenticateDevice is marked as deprecated.
// BluetoothAuthenticateDeviceEx requires a bunch of rigmarole.. maybe some other day.
#pragma warning(push)
#pragma warning(disable : 4995)
bool AuthenticateWiimote(HANDLE radio_handle, const BLUETOOTH_RADIO_INFO& radio_info,
                         BLUETOOTH_DEVICE_INFO_STRUCT* btdi, AuthenticationMethod auth_method)
{
  // When pressing the sync button, the remote expects the host's address as the pass key.
  // When pressing 1+2 it expects its own address.
  // I'm not sure if there is a convenient way to determine which one the remote wants?
  // And using the wrong method makes Windows immediately disconnect the remote..
  const auto& bdaddr_to_use =
      (auth_method == AuthenticationMethod::SyncButton) ? radio_info.address : btdi->Address;

  // The Bluetooth device address is stored in the typical order (reverse of display order).
  std::array<WCHAR, sizeof(bdaddr_to_use.rgBytes)> pass_key;
  std::ranges::copy(bdaddr_to_use.rgBytes, pass_key.data());

  const DWORD auth_result = BluetoothAuthenticateDevice(nullptr, radio_handle, btdi,
                                                        pass_key.data(), ULONG(pass_key.size()));
  if (ERROR_SUCCESS != auth_result)
  {
    // FYI: Tends to fail with ERROR_NO_MORE_ITEMS or ERROR_GEN_FAILURE.
    ERROR_LOG_FMT(WIIMOTE, "BluetoothAuthenticateDevice failed: {}", auth_result);
    return false;
  }

  // Apparently must be done to make the remote remember the pairing.
  DWORD pc_services{};
  const DWORD services_result =
      BluetoothEnumerateInstalledServices(radio_handle, btdi, &pc_services, nullptr);
  if (services_result != ERROR_SUCCESS && services_result != ERROR_MORE_DATA)
  {
    ERROR_LOG_FMT(WIIMOTE, "BluetoothEnumerateInstalledServices failed: {}", services_result);
    return false;
  }

  return true;
}
#pragma warning(pop)

std::optional<USBUtils::DeviceInfo> GetDeviceInfo(const WCHAR* hid_iface)
{
  // libusb opens without read/write access to get attributes, so we'll do that too.
  constexpr auto open_access = 0;
  constexpr auto open_flags = FILE_SHARE_READ | FILE_SHARE_WRITE;
  const auto dev_handle = CreateFile(hid_iface, open_access, open_flags, nullptr, OPEN_EXISTING,
                                     FILE_FLAG_OVERLAPPED, nullptr);
  if (dev_handle == INVALID_HANDLE_VALUE)
  {
    WARN_LOG_FMT(WIIMOTE, "CreateFile");
    return std::nullopt;
  }

  HIDD_ATTRIBUTES attributes{.Size = sizeof(attributes)};
  if (!HidD_GetAttributes(dev_handle, &attributes))
  {
    ERROR_LOG_FMT(WIIMOTE, "HidD_GetAttributes");
    return std::nullopt;
  }

  return USBUtils::DeviceInfo{attributes.VendorID, attributes.ProductID};
}

static std::optional<std::string> GetParentDeviceDescription(const WCHAR* hid_iface)
{
  auto dev_inst_id =
      Common::GetDeviceInterfaceStringProperty(hid_iface, &DEVPKEY_Device_InstanceId);

  if (!dev_inst_id.has_value())
    return std::nullopt;

  constexpr ULONG locate_flags = CM_LOCATE_DEVNODE_NORMAL;
  DEVINST dev_inst{};
  if (CM_Locate_DevNode(&dev_inst, dev_inst_id->data(), locate_flags) != CR_SUCCESS)
  {
    ERROR_LOG_FMT(WIIMOTE, "CM_Locate_DevNode");
    return std::nullopt;
  }

  DEVINST parent_inst{};
  if (CM_Get_Parent(&parent_inst, dev_inst, 0) != CR_SUCCESS)
  {
    ERROR_LOG_FMT(WIIMOTE, "CM_Get_Parent");
    return std::nullopt;
  }

  const auto description =
      Common::GetDevNodeStringProperty(parent_inst, &DEVPKEY_Device_BusReportedDeviceDesc);

  if (description.has_value())
    return WStringToUTF8(*description);

  return std::nullopt;
}

void EnumerateRadios(std::invocable<HANDLE> auto&& enumeration_callback)
{
  constexpr BLUETOOTH_FIND_RADIO_PARAMS radio_params{
      .dwSize = sizeof(radio_params),
  };

  HANDLE radio_handle{};
  const auto find_radio = BluetoothFindFirstRadio(&radio_params, &radio_handle);
  if (find_radio == nullptr)
  {
    ERROR_LOG_FMT(WIIMOTE, "BluetoothFindFirstRadio: {}", Common::GetLastErrorString());
    return;
  }
  Common::ScopeGuard find_guard([=] { BluetoothFindRadioClose(find_radio); });

  while (true)
  {
    Common::ScopeGuard radio_guard([=] { CloseHandle(radio_handle); });

    if (std::invoke(enumeration_callback, radio_handle) == EnumerationControl::Stop)
      break;

    if (!BluetoothFindNextRadio(find_radio, &radio_handle))
      break;
  }
}

// Use inquiry_length of 0 to enumerate known devices.
void EnumerateBluetoothDevices(u8 inquiry_length, auto&& enumeration_callback)
{
  BLUETOOTH_DEVICE_SEARCH_PARAMS search_params{
      .dwSize = sizeof(search_params),
      .fReturnAuthenticated = true,
      .fReturnRemembered = true,
      .fReturnUnknown = true,
      .fReturnConnected = true,
      .fIssueInquiry = inquiry_length > 0,
      .cTimeoutMultiplier = inquiry_length,
  };

  EnumerateRadios([&](HANDLE radio_handle) {
    BLUETOOTH_RADIO_INFO radio_info{.dwSize = sizeof(radio_info)};
    if (BluetoothGetRadioInfo(radio_handle, &radio_info) != ERROR_SUCCESS)
    {
      ERROR_LOG_FMT(WIIMOTE, "BluetoothGetRadioInfo");
      return EnumerationControl::Continue;
    }

    search_params.hRadio = radio_handle;

    BLUETOOTH_DEVICE_INFO btdi{.dwSize = sizeof(btdi)};
    const auto find_device = BluetoothFindFirstDevice(&search_params, &btdi);
    if (find_device == nullptr)
    {
      const auto find_device_error = GetLastError();
      if (find_device_error != ERROR_NO_MORE_ITEMS)
      {
        ERROR_LOG_FMT(WIIMOTE, "BluetoothFindFirstDevice: {}",
                      Common::GetWin32ErrorString(find_device_error));
      }
      return EnumerationControl::Continue;
    }
    Common::ScopeGuard find_guard([=] { BluetoothFindDeviceClose(find_device); });

    while (true)
    {
      if (enumeration_callback(radio_handle, radio_info, &btdi) == EnumerationControl::Stop)
        break;

      if (!BluetoothFindNextDevice(find_device, &btdi))
        break;
    }
    return EnumerationControl::Continue;
  });
}

u32 RemoveWiimoteBluetoothDevices(std::invocable<BLUETOOTH_DEVICE_INFO> auto&& should_remove_filter)
{
  u32 remove_count = 0;
  EnumerateBluetoothWiimotes(0, [&](HANDLE radio_handle, const auto& radio_info, auto* btdi) {
    if (!std::invoke(should_remove_filter, *btdi))
      return EnumerationControl::Continue;

    NOTICE_LOG_FMT(WIIMOTE, "Removing Wiimote device.");

    const auto remove_device_result = BluetoothRemoveDevice(&btdi->Address);
    if (remove_device_result != ERROR_SUCCESS)
    {
      ERROR_LOG_FMT(WIIMOTE, "BluetoothRemoveDevice failed: {}", remove_device_result);
      return EnumerationControl::Continue;
    }

    ++remove_count;
    return EnumerationControl::Continue;
  });

  return remove_count;
}

// Windows is problematic with remembering disconnected Wii remotes.
// If they are authenticated, the remote can reestablish the connection with any button.
// If they are *not* authenticated there's apparently no feasible way to reconnect them.
// We remove these problematic remembered devices so we can reconnect them.
// Otherwise, the user would need to manually deleting the device in control panel.
u32 RemoveUnusableWiimoteBluetoothDevices()
{
  return RemoveWiimoteBluetoothDevices([](const BLUETOOTH_DEVICE_INFO& btdi) {
    return btdi.fRemembered && !btdi.fConnected && !btdi.fAuthenticated;
  });
}

u32 DiscoverAndPairWiimotes(u8 inquiry_length,
                            std::optional<AuthenticationMethod> auth_method = std::nullopt)
{
  u32 success_count = 0;
  EnumerateBluetoothWiimotes(
      inquiry_length, [&](HANDLE radio_handle, const auto& radio_info, auto* btdi) {
        if (btdi->fConnected)
          return EnumerationControl::Continue;

        const auto name = WStringToUTF8(btdi->szName);
        INFO_LOG_FMT(WIIMOTE, "Found Bluetooth device with name: {}", name);

        if (!btdi->fAuthenticated && auth_method.has_value())
        {
          INFO_LOG_FMT(WIIMOTE, "Authenticating Wiimote");
          AuthenticateWiimote(radio_handle, radio_info, btdi, *auth_method);
        }

        INFO_LOG_FMT(WIIMOTE, "Enabling HID service on Wiimote");
        const auto service_state_result = BluetoothSetServiceState(
            radio_handle, btdi, &HumanInterfaceDeviceServiceClass_UUID, BLUETOOTH_SERVICE_ENABLE);
        if (service_state_result != ERROR_SUCCESS)
        {
          // FYI: Tends to fail with ERROR_INVALID_PARAMETER.
          ERROR_LOG_FMT(WIIMOTE, "BluetoothSetServiceState failed: {}", service_state_result);
          return EnumerationControl::Continue;
        }

        ++success_count;
        return EnumerationControl::Continue;
      });

  return success_count;
}

void WiimoteScannerWindows::FindAndAuthenticateWiimotes()
{
  // The sync button method conveniently makes remotes seek reconnection on button press.
  // I think the 1+2 method is effectively pointless?
  static constexpr auto auth_method = AuthenticationMethod::SyncButton;

  // Windows isn't so cooperative. This helps one button click actually able to pair a remote.
  constexpr int ITERATION_COUNT = 3;

  RemoveUnusableWiimoteBluetoothDevices();

  auto pair_count = 0;
  for (int i = 0; i != ITERATION_COUNT; ++i)
    pair_count += DiscoverAndPairWiimotes(DEFAULT_INQUIRY_LENGTH, auth_method);

  NOTICE_LOG_FMT(WIIMOTE, "Successfully paired Wiimotes: {}", pair_count);
}

void WiimoteScannerWindows::RemoveRememberedWiimotes()
{
  const auto forget_count = RemoveWiimoteBluetoothDevices(&BLUETOOTH_DEVICE_INFO::fRemembered);
  NOTICE_LOG_FMT(WIIMOTE, "Removed remembered Wiimotes: {}", forget_count);
}

WiimoteScannerWindows::WiimoteScannerWindows() = default;

void WiimoteScannerWindows::Update()
{
}

// See http://wiibrew.org/wiki/Wiimote for the Report IDs and its sizes
size_t GetReportSize(u8 rid)
{
  using namespace WiimoteCommon;

  switch (const auto report_id = static_cast<InputReportID>(rid))
  {
  case InputReportID::Status:
    return sizeof(InputReportStatus);
  case InputReportID::ReadDataReply:
    return sizeof(InputReportReadDataReply);
  case InputReportID::Ack:
    return sizeof(InputReportAck);
  default:
    if (DataReportBuilder::IsValidMode(report_id))
      return MakeDataReportManipulator(report_id, nullptr)->GetDataSize();
    else
      return 0;
  }
}

WiimoteWindows::WiimoteWindows(std::wstring hid_iface) : m_hid_iface{std::move(hid_iface)}
{
  constexpr bool manual_reset = false;
  constexpr bool initial_state = false;
  m_hid_overlap_read.hEvent = CreateEvent(nullptr, manual_reset, initial_state, nullptr);
  m_hid_overlap_write.hEvent = CreateEvent(nullptr, manual_reset, initial_state, nullptr);
  m_wakeup_event = CreateEvent(nullptr, manual_reset, initial_state, nullptr);
}

WiimoteWindows::~WiimoteWindows()
{
  Shutdown();
  CloseHandle(m_hid_overlap_read.hEvent);
  CloseHandle(m_hid_overlap_write.hEvent);
  CloseHandle(m_wakeup_event);
}

bool WiimoteWindows::ConnectInternal()
{
  if (IsConnected())
    return true;

  // TODO: Try without shared? Would that stop Steam from messing with things?
  constexpr auto open_flags = FILE_SHARE_READ | FILE_SHARE_WRITE;
  m_dev_handle = CreateFile(m_hid_iface.c_str(), GENERIC_READ | GENERIC_WRITE, open_flags, nullptr,
                            OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

  if (m_dev_handle == INVALID_HANDLE_VALUE)
    return false;

  // Note: Windows keeps devices around for disconnected Wii remotes.
  //
  // Windows 11 Microsoft Bluetooth stack behavior:
  // Even minutes after a disconnect, CreateFile will succeed and reads will just time out.
  // But the first write will immediately fail with ERROR_OPERATION_ABORTED.
  // After that write the interface seems to properly enumerate as not present.
  //
  // The Mayflash DolphinBar behaves similarly, but CreateFile always succeeds.
  // And write attempts immediately produce ERROR_GEN_FAILURE.
  //
  // TLDR: We need to do a write to see if a Wii remote is actually connected.

  // Attempt a write. We don't care about the response right now.
  u8 const req_status_report[] = {u8(WiimoteCommon::OutputReportID::RequestStatus), 0};
  const auto write_result = OverlappedWrite(req_status_report, sizeof(req_status_report));

  if (write_result <= 0)
  {
    CloseHandle(std::exchange(m_dev_handle, nullptr));
    return false;
  }

  m_is_connected.store(true, std::memory_order_relaxed);
  return true;
}

void WiimoteWindows::DisconnectInternal()
{
  if (!IsConnected())
    return;

  m_is_connected.store(false, std::memory_order_relaxed);
  CloseHandle(std::exchange(m_dev_handle, nullptr));
}

std::string WiimoteWindows::GetId() const
{
  return WStringToUTF8(m_hid_iface);
}

bool WiimoteWindows::IsConnected() const
{
  return m_is_connected.load(std::memory_order_relaxed);
}

int WiimoteWindows::OverlappedRead(u8* data, DWORD size)
{
  DWORD bytes_read = 0;
  if (ReadFile(m_dev_handle, data, size, &bytes_read, &m_hid_overlap_read))
    return bytes_read;

  auto const read_error = GetLastError();
  if (ERROR_IO_PENDING != read_error)
  {
    ERROR_LOG_FMT(WIIMOTE, "ReadFile on Wiimote {}: {}", m_index + 1,
                  Common::GetWin32ErrorString(read_error));
    return 0;
  }

  // Wait for either our wakeup event or the above I/O operation.
  const std::array<HANDLE, 2> objs{m_wakeup_event, m_hid_overlap_read.hEvent};
  constexpr bool wait_for_all = false;
  const auto wait_result = WaitForMultipleObjects(DWORD(objs.size()), objs.data(), wait_for_all,
                                                  WIIMOTE_DEFAULT_TIMEOUT);
  bool io_pending = true;
  switch (wait_result)
  {
  case WAIT_OBJECT_0:
    // Handle IOWakeup.
    DEBUG_LOG_FMT(WIIMOTE, "IOWakeup");
    break;
  case WAIT_OBJECT_0 + 1:
    // The I/O operation is complete.
    io_pending = false;
    break;
  case WAIT_TIMEOUT:
    DEBUG_LOG_FMT(WIIMOTE, "OverlappedRead: WAIT_TIMEOUT");
    break;
  default:
    ERROR_LOG_FMT(WIIMOTE, "OverlappedRead WaitForMultipleObjects: {}",
                  Common::GetLastErrorString());
    break;
  }

  if (io_pending)
    CancelIo(m_dev_handle);

  // Once here, either the I/O is ready or being cancelled from just above.
  if (GetOverlappedResult(m_dev_handle, &m_hid_overlap_read, &bytes_read, io_pending))
    return bytes_read;

  const auto overlapped_error = GetLastError();

  // This is okay. It happens on read timeout or IOWakeup.
  if (overlapped_error == ERROR_OPERATION_ABORTED)
    return -1;

  DEBUG_LOG_FMT(WIIMOTE, "GetOverlappedResult: {}", Common::GetWin32ErrorString(overlapped_error));
  return 0;
}

// positive = read packet
// negative = didn't read packet
// zero = error
int WiimoteWindows::IORead(u8* buf)
{
  // We need to prepend an HID Profile byte that Windows doesn't include.
  constexpr u8 HID_DATA_INPUT = 0xa1;
  buf[0] = HID_DATA_INPUT;

  const auto read_result = OverlappedRead(buf + 1, MAX_PAYLOAD - 1);

  DEBUG_LOG_FMT(WIIMOTE, "OverlappedRead bytes: {}", read_result);

  if (read_result <= 0)
    return read_result;

  // ReadFile will always return 22 bytes read.
  // So we need to calculate the actual report size by its report ID.
  const u8 report_id = buf[1];
  const auto report_size = static_cast<DWORD>(GetReportSize(report_id));

  if (report_size == 0)
  {
    ERROR_LOG_FMT(WIIMOTE, "Received unknown report on Wiimote {}: 0x{:02x}", m_index + 1,
                  report_id);
    return -1;
  }

  return sizeof(HID_DATA_INPUT) + sizeof(report_id) + report_size;
}

void WiimoteWindows::IOWakeup()
{
  SetEvent(m_wakeup_event);
}

int WiimoteWindows::OverlappedWrite(const u8* data, DWORD size)
{
  DWORD bytes_written = 0;
  if (WriteFile(m_dev_handle, data, size, &bytes_written, &m_hid_overlap_write))
    return bytes_written;

  auto const write_error = GetLastError();
  if (ERROR_IO_PENDING != write_error)
  {
    ERROR_LOG_FMT(WIIMOTE, "WriteFile on Wiimote {}: {}", m_index + 1,
                  Common::GetWin32ErrorString(write_error));
    return 0;
  }

  const auto wait_result = WaitForSingleObject(m_hid_overlap_write.hEvent, WIIMOTE_DEFAULT_TIMEOUT);

  bool io_pending = true;
  switch (wait_result)
  {
  case WAIT_OBJECT_0:
    // The I/O operation is complete.
    io_pending = false;
    break;
  default:
    ERROR_LOG_FMT(WIIMOTE, "OverlappedWrite WaitForSingleObject: {}", Common::GetLastErrorString());
    break;
  }

  if (io_pending)
    CancelIo(m_dev_handle);

  if (GetOverlappedResult(m_dev_handle, &m_hid_overlap_write, &bytes_written, io_pending))
    return bytes_written;

  // Note: DolphinBar's disconnected remotes produce ERROR_GEN_FAILURE here.
  DEBUG_LOG_FMT(WIIMOTE, "OverlappedWrite GetOverlappedResult: {}", Common::GetLastErrorString());
  return 0;
}

int WiimoteWindows::IOWrite(const u8* buf, size_t len)
{
  assert(len > 0);

  // Skip the HID Profile byte. Windows handles that on its own.
  const u8* const write_buffer = buf + 1;
  const DWORD bytes_to_write = DWORD(len - 1);

  const auto write_result = OverlappedWrite(write_buffer, bytes_to_write);

  DEBUG_LOG_FMT(WIIMOTE, "OverlappedWrite bytes: {}", write_result);

  return write_result;
}

auto WiimoteScannerWindows::FindWiimoteHIDDevices() -> FindResults
{
  FindResults results;

  // Enumerate connected HID interfaces IDs.
  auto class_guid = GUID_DEVINTERFACE_HID;
  constexpr ULONG flags = CM_GET_DEVICE_INTERFACE_LIST_PRESENT;
  ULONG list_size = 0;
  CM_Get_Device_Interface_List_Size(&list_size, &class_guid, nullptr, flags);

  const auto buffer = std::make_unique_for_overwrite<WCHAR[]>(list_size);
  const auto list_result =
      CM_Get_Device_Interface_List(&class_guid, nullptr, buffer.get(), list_size, flags);
  if (list_result != CR_SUCCESS)
  {
    ERROR_LOG_FMT(WIIMOTE, "CM_Get_Device_Interface_List: {}", list_result);
    return results;
  }

  for (const WCHAR* hid_iface = buffer.get(); *hid_iface != L'\0';
       hid_iface += wcslen(hid_iface) + 1)
  {
    // TODO: WiimoteWindows::GetId() does a redundant conversion.
    const auto hid_iface_utf8 = WStringToUTF8(hid_iface);
    DEBUG_LOG_FMT(WIIMOTE, "Found HID interface: {}", hid_iface_utf8);

    // Are we already using this device?
    if (!IsNewWiimote(hid_iface_utf8))
      continue;

    // When connected via Bluetooth, this has a proper name like "Nintendo RVL-CNT-01".
    const auto parent_description = GetParentDeviceDescription(hid_iface);

    if (parent_description.has_value())
      DEBUG_LOG_FMT(WIIMOTE, "HID description: {}", *parent_description);

    // Mayflash has confirmed in email that every revision of the DolphinBar
    //  advertises this descriptor and a VID:PID of 057e:0306.
    constexpr auto dolphinbar_device_description = "Mayflash Wiimote PC Adapter";

    // TODO: If we wanted to be fancy,
    //  it might be possible to determine which "slot" of the DolphinBar this is.
    // It really ultimately doesn't matter, but it could keep the
    //  player LEDs consistent after the DolphinBar assigns them.
    // Alternatively, it might be easier to just request a status report
    //  to read back the current LED state.

    // Sometimes we can determine this from the device description.
    std::optional<bool> is_balance_board;

    bool is_relevant_description = false;
    if (parent_description.has_value())
    {
      if (IsBalanceBoardName(*parent_description))
      {
        is_relevant_description = true;
        is_balance_board = true;
      }
      else if (IsWiimoteName(*parent_description))
      {
        is_relevant_description = true;
        is_balance_board = false;
      }
      else if (*parent_description == dolphinbar_device_description)
      {
        is_relevant_description = true;
      }
    }

    // Whelp, if the description didn't match, let's check the VID/PID ?
    // This is potentially unnecessary. Checking the description should be enough.
    // FYI: I think some off brand Wii remotes have different IDs.
    // IIRC: They all do spoof the description because the Wii only checks that.
    if (!is_relevant_description)
    {
      const auto dev_info = GetDeviceInfo(hid_iface);
      if (!dev_info.has_value())
        continue;

      DEBUG_LOG_FMT(WIIMOTE, "HID VID/PID: {}", dev_info->ToString());

      if (!IsKnownDeviceId(*dev_info))
        continue;
    }

    // Once here, we are confident that this is a Wii device.

    DEBUG_LOG_FMT(WIIMOTE, "Creating WiimoteWindows");

    auto wiimote = std::make_unique<WiimoteWindows>(hid_iface);
    if (!wiimote->ConnectInternal())
    {
      // This happens frequently.
      // Windows and DolphinBar both expose interfaces for non-connected Wii Remotes.
      DEBUG_LOG_FMT(WIIMOTE, "ConnectInternal failed");
      continue;
    }

    // If we didn't learn it from the name, run the balance board extension check.
    if (!is_balance_board.has_value())
      is_balance_board = wiimote->IsBalanceBoard();

    if (*is_balance_board)
      results.balance_boards.emplace_back(std::move(wiimote));
    else
      results.wii_remotes.emplace_back(std::move(wiimote));
  }

  return results;
}

auto WiimoteScannerWindows::FindNewWiimotes() -> FindResults
{
  // Ideally we'd only enumerate the radios once.
  RemoveUnusableWiimoteBluetoothDevices();
  DiscoverAndPairWiimotes(DEFAULT_INQUIRY_LENGTH);

  return FindWiimoteHIDDevices();
}

auto WiimoteScannerWindows::FindAttachedWiimotes() -> FindResults
{
  return FindWiimoteHIDDevices();
}

bool WiimoteScannerWindows::IsReady() const
{
  bool found_radio = false;
  EnumerateRadios([&](auto) {
    found_radio = true;
    return EnumerationControl::Stop;
  });
  return found_radio;
}

}  // namespace WiimoteReal
