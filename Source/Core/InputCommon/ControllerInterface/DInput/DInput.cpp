// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/DInput/DInput.h"

#include "Common/HRWrap.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/DInput/DInputJoystick.h"
#include "InputCommon/ControllerInterface/DInput/DInputKeyboardMouse.h"

#pragma comment(lib, "Dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace ciface::DInput
{
static IDirectInput8* s_idi8 = nullptr;

BOOL CALLBACK DIEnumDeviceObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef)
{
  ((std::list<DIDEVICEOBJECTINSTANCE>*)pvRef)->push_back(*lpddoi);
  return DIENUM_CONTINUE;
}

BOOL CALLBACK DIEnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
  ((std::list<DIDEVICEINSTANCE>*)pvRef)->push_back(*lpddi);
  return DIENUM_CONTINUE;
}

std::string GetDeviceName(const LPDIRECTINPUTDEVICE8 device)
{
  DIPROPSTRING str = {};
  str.diph.dwSize = sizeof(str);
  str.diph.dwHeaderSize = sizeof(str.diph);
  str.diph.dwHow = DIPH_DEVICE;

  std::string result;
  HRESULT hr = device->GetProperty(DIPROP_PRODUCTNAME, &str.diph);
  if (SUCCEEDED(hr))
  {
    result = StripWhitespace(WStringToUTF8(str.wsz));
  }
  else
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "GetProperty(DIPROP_PRODUCTNAME) failed: {}",
                  Common::HRWrap(hr));
  }

  return result;
}

// Assumes hwnd had not changed from the previous call
void PopulateDevices(HWND hwnd)
{
  if (!s_idi8)
  {
    HRESULT hr = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION,
                                    IID_IDirectInput8, (LPVOID*)&s_idi8, nullptr);
    if (FAILED(hr))
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "DirectInput8Create failed: {}", Common::HRWrap(hr));
      return;
    }
  }

  // Remove old (invalid) devices. No need to ever remove the KeyboardMouse device.
  // Note that if we have 2+ DInput controllers, not fully repopulating devices
  // will mean that a device with index "2" could persist while there is no device with index "0".
  // This is slightly inconsistent as when we refresh all devices, they will instead reset, and
  // that happens a lot (for uncontrolled reasons, like starting/stopping the emulation).
  g_controller_interface.RemoveDevice(
      [](const auto* dev) { return dev->GetSource() == DINPUT_SOURCE_NAME && !dev->IsValid(); });

  InitKeyboardMouse(s_idi8, hwnd);
  InitJoystick(s_idi8, hwnd);
}

void ChangeWindow(HWND hwnd)
{
  if (s_idi8)  // Has init? Ignore if called before the first PopulateDevices()
  {
    // The KeyboardMouse device is marked as virtual device, so we avoid removing it.
    // We need to force all the DInput joysticks to be destroyed now, or recreation would fail.
    g_controller_interface.RemoveDevice(
        [](const auto* dev) {
          return dev->GetSource() == DINPUT_SOURCE_NAME && !dev->IsVirtualDevice();
        },
        true);

    SetKeyboardMouseWindow(hwnd);
    InitJoystick(s_idi8, hwnd);
  }
}

void DeInit()
{
  if (s_idi8)
  {
    s_idi8->Release();
    s_idi8 = nullptr;
  }
}
}  // namespace ciface::DInput
