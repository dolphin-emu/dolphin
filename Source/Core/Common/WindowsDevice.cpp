// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32

#include "Common/WindowsDevice.h"

#include <string>

#include "Hidclass.h"

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"

namespace Common
{
std::optional<std::wstring> GetPropertyHelper(auto function, auto dev,
                                              const DEVPROPKEY* requested_property,
                                              DEVPROPTYPE expected_type)
{
  DEVPROPTYPE type{};
  ULONG buffer_size{};

  if (const auto result = function(dev, requested_property, &type, nullptr, &buffer_size, 0);
      result != CR_SUCCESS && result != CR_BUFFER_SMALL)
  {
    if (result != CR_NO_SUCH_VALUE)
      WARN_LOG_FMT(COMMON, "CM_Get_DevNode_Property returned: {}", result);
    return std::nullopt;
  }
  if (type != expected_type)
  {
    WARN_LOG_FMT(COMMON, "CM_Get_DevNode_Property unexpected type: 0x{:x}", type);
    return std::nullopt;
  }

  std::optional<std::wstring> property;
  // FYI: It's legal to write the null terminator at data()[size()] of std::basic_string.
  property.emplace(buffer_size / sizeof(WCHAR) - 1, L'\0');
  if (const auto result = function(dev, requested_property, &type,
                                   reinterpret_cast<BYTE*>(property->data()), &buffer_size, 0);
      result != CR_SUCCESS)
  {
    ERROR_LOG_FMT(COMMON, "CM_Get_DevNode_Property returned: {}", result);
    return std::nullopt;
  }
  return property;
}

std::optional<std::wstring> GetDevNodeStringProperty(DEVINST dev,
                                                     const DEVPROPKEY* requested_property)
{
  return GetPropertyHelper(CM_Get_DevNode_Property, dev, requested_property, DEVPROP_TYPE_STRING);
}

std::optional<std::wstring> GetDeviceInterfaceStringProperty(LPCWSTR iface,
                                                             const DEVPROPKEY* requested_property)
{
  return GetPropertyHelper(CM_Get_Device_Interface_Property, iface, requested_property,
                           DEVPROP_TYPE_STRING);
}

NullTerminatedStringList<WCHAR> GetDeviceInterfaceList(LPGUID iface_class_guid, DEVINSTID device_id,
                                                       ULONG flags)
{
  while (true)
  {
    ULONG list_size = 0;
    const auto size_result =
        CM_Get_Device_Interface_List_Size(&list_size, iface_class_guid, device_id, flags);
    if (size_result != CR_SUCCESS || list_size == 0)
      list_size = 1;

    auto buffer = std::make_unique_for_overwrite<WCHAR[]>(list_size);
    const auto list_result =
        CM_Get_Device_Interface_List(iface_class_guid, device_id, buffer.get(), list_size, flags);

    // "A new device can be added to the system causing the size returned to no longer be valid."
    // Microsoft recommends trying again in a loop.
    if (list_result == CR_BUFFER_SMALL)
      continue;

    if (list_result != CR_SUCCESS)
    {
      ERROR_LOG_FMT(COMMON, "CM_Get_Device_Interface_List: {}", list_result);
      buffer[0] = 0;
    }

    return {std::move(buffer)};
  }
}

static __callback DWORD OnDevicesChanged(_In_ HCMNOTIFICATION notify_handle, _In_opt_ PVOID context,
                                         _In_ CM_NOTIFY_ACTION action,
                                         _In_reads_bytes_(event_data_size)
                                             PCM_NOTIFY_EVENT_DATA event_data,
                                         _In_ DWORD event_data_size)
{
  auto& callback = *static_cast<DeviceChangeNotification::CallbackType*>(context);
  switch (action)
  {
  case CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL:
    callback(DeviceChangeNotification::EventType::Arrival);
    break;
  case CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL:
    callback(DeviceChangeNotification::EventType::Removal);
    break;
  default:
    break;
  }
  return ERROR_SUCCESS;
}

DeviceChangeNotification::DeviceChangeNotification() = default;

DeviceChangeNotification::~DeviceChangeNotification()
{
  Unregister();
}

void DeviceChangeNotification::Register(CallbackType callback)
{
  Unregister();
  m_callback = std::move(callback);

  CM_NOTIFY_FILTER notify_filter{
      .cbSize = sizeof(notify_filter),
      .FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE,
      .u{.DeviceInterface{.ClassGuid = GUID_DEVINTERFACE_HID}},
  };
  const CONFIGRET cfg_rv =
      CM_Register_Notification(&notify_filter, &m_callback, OnDevicesChanged, &m_notify_handle);
  if (cfg_rv != CR_SUCCESS)
  {
    ERROR_LOG_FMT(COMMON, "CM_Register_Notification failed: {:x}", cfg_rv);
  }
}

void DeviceChangeNotification::Unregister()
{
  if (m_notify_handle == nullptr)
    return;

  const CONFIGRET cfg_rv = CM_Unregister_Notification(m_notify_handle);
  if (cfg_rv != CR_SUCCESS)
  {
    ERROR_LOG_FMT(COMMON, "CM_Unregister_Notification failed: {:x}", cfg_rv);
  }
  m_notify_handle = nullptr;
}

}  // namespace Common

#endif
