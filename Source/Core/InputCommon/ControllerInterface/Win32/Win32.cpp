// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Win32/Win32.h"

#include <Windows.h>
#include <cfgmgr32.h>
// must be before hidclass
#include <initguid.h>

#include <hidclass.h>

#include <mutex>

#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "InputCommon/ControllerInterface/DInput/DInput.h"
#include "InputCommon/ControllerInterface/WGInput/WGInput.h"
#include "InputCommon/ControllerInterface/XInput/XInput.h"

#pragma comment(lib, "OneCoreUAP.Lib")

static std::mutex s_populate_mutex;
// TODO is this really needed?
static Common::Flag s_first_populate_devices_asked;
static HCMNOTIFICATION s_notify_handle;

namespace ciface::Win32
{
class InputBackend final : public ciface::InputBackend
{
public:
  InputBackend(ControllerInterface* controller_interface);
  ~InputBackend();

  void PopulateDevices() override;
  void HandleWindowChange() override;
  HWND GetHWND();
};
}  // namespace ciface::Win32

_Pre_satisfies_(EventDataSize >= sizeof(CM_NOTIFY_EVENT_DATA)) static DWORD CALLBACK
    OnDevicesChanged(_In_ HCMNOTIFICATION hNotify, _In_opt_ PVOID Context,
                     _In_ CM_NOTIFY_ACTION Action,
                     _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
                     _In_ DWORD EventDataSize)
{
  if (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL ||
      Action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
  {
    // Windows automatically sends this message before we ask for it and before we are "ready" to
    // listen for it.
    if (s_first_populate_devices_asked.IsSet())
    {
      std::lock_guard lk_population(s_populate_mutex);
      // TODO: we could easily use the message passed alongside this event, which tells
      // whether a device was added or removed, to avoid removing old, still connected, devices
      g_controller_interface.PlatformPopulateDevices([&] {
        ciface::DInput::PopulateDevices(
            static_cast<ciface::Win32::InputBackend*>(Context)->GetHWND());
        ciface::XInput::PopulateDevices();
      });
    }
  }
  return ERROR_SUCCESS;
}

namespace ciface::Win32
{
std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface)
{
  return std::make_unique<InputBackend>(controller_interface);
}

HWND InputBackend::GetHWND()
{
  return static_cast<HWND>(GetControllerInterface().GetWindowSystemInfo().render_window);
}

InputBackend::InputBackend(ControllerInterface* controller_interface)
    : ciface::InputBackend(controller_interface)
{
  XInput::Init();
  WGInput::Init();

  CM_NOTIFY_FILTER notify_filter{.cbSize = sizeof(notify_filter),
                                 .FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE,
                                 .u{.DeviceInterface{.ClassGuid = GUID_DEVINTERFACE_HID}}};
  const CONFIGRET cfg_rv =
      CM_Register_Notification(&notify_filter, this, OnDevicesChanged, &s_notify_handle);
  if (cfg_rv != CR_SUCCESS)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "CM_Register_Notification failed: {:x}", cfg_rv);
  }
}

void InputBackend::PopulateDevices()
{
  std::lock_guard lk_population(s_populate_mutex);
  s_first_populate_devices_asked.Set();
  ciface::DInput::PopulateDevices(GetHWND());
  ciface::XInput::PopulateDevices();
  ciface::WGInput::PopulateDevices();
}

void InputBackend::HandleWindowChange()
{
  std::lock_guard lk_population(s_populate_mutex);
  ciface::DInput::ChangeWindow(GetHWND());
}

InputBackend::~InputBackend()
{
  s_first_populate_devices_asked.Clear();
  DInput::DeInit();

  if (s_notify_handle)
  {
    const CONFIGRET cfg_rv = CM_Unregister_Notification(s_notify_handle);
    if (cfg_rv != CR_SUCCESS)
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "CM_Unregister_Notification failed: {:x}", cfg_rv);
    }
    s_notify_handle = nullptr;
  }

  XInput::DeInit();
  WGInput::DeInit();
}

}  // namespace ciface::Win32
