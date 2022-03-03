// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Win32/Win32.h"

#include <windows.h>

#include <array>
#include <future>
#include <mutex>
#include <thread>

#include "Common/Flag.h"
#include "Common/HRWrap.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/Thread.h"
#include "InputCommon/ControllerInterface/DInput/DInput.h"
#include "InputCommon/ControllerInterface/XInput/XInput.h"

constexpr UINT WM_DOLPHIN_STOP = WM_USER;

// Dolphin's render window
static HWND s_hwnd;
// Windows messaging window (hidden)
static HWND s_message_window;
static std::thread s_thread;
static std::mutex s_populate_mutex;
static Common::Flag s_first_populate_devices_asked;

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  if (message == WM_INPUT_DEVICE_CHANGE)
  {
    // Windows automatically sends this message before we ask for it and before we are "ready" to
    // listen for it.
    if (s_first_populate_devices_asked.IsSet())
    {
      std::lock_guard lk_population(s_populate_mutex);
      // TODO: we could easily use the message passed alongside this event, which tells
      // whether a device was added or removed, to avoid removing old, still connected, devices
      g_controller_interface.PlatformPopulateDevices([] {
        ciface::DInput::PopulateDevices(s_hwnd);
        ciface::XInput::PopulateDevices();
      });
    }
  }

  return DefWindowProc(hwnd, message, wparam, lparam);
}

void ciface::Win32::Init(void* hwnd)
{
  s_hwnd = static_cast<HWND>(hwnd);
  XInput::Init();

  std::promise<HWND> message_window_promise;

  s_thread = std::thread([&message_window_promise] {
    Common::SetCurrentThreadName("ciface::Win32 Message Loop");

    HWND message_window = nullptr;
    Common::ScopeGuard promise_guard([&] { message_window_promise.set_value(message_window); });

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "CoInitializeEx failed: {}", Common::HRWrap(hr));
      return;
    }
    Common::ScopeGuard uninit([] { CoUninitialize(); });

    WNDCLASSEX window_class_info{};
    window_class_info.cbSize = sizeof(window_class_info);
    window_class_info.lpfnWndProc = WindowProc;
    window_class_info.hInstance = GetModuleHandle(nullptr);
    window_class_info.lpszClassName = L"Message";

    ATOM window_class = RegisterClassEx(&window_class_info);
    if (!window_class)
    {
      NOTICE_LOG_FMT(CONTROLLERINTERFACE, "RegisterClassEx failed: {}",
                     Common::HRWrap(GetLastError()));
      return;
    }
    Common::ScopeGuard unregister([&window_class] {
      if (!UnregisterClass(MAKEINTATOM(window_class), GetModuleHandle(nullptr)))
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "UnregisterClass failed: {}",
                      Common::HRWrap(GetLastError()));
    });

    message_window = CreateWindowEx(0, L"Message", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                    nullptr, nullptr);
    promise_guard.Exit();
    if (!message_window)
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "CreateWindowEx failed: {}",
                    Common::HRWrap(GetLastError()));
      return;
    }
    Common::ScopeGuard destroy([&] {
      if (!DestroyWindow(message_window))
        ERROR_LOG_FMT(CONTROLLERINTERFACE, "DestroyWindow failed: {}",
                      Common::HRWrap(GetLastError()));
    });

    std::array<RAWINPUTDEVICE, 2> devices;
    // game pad devices
    devices[0].usUsagePage = 0x01;
    devices[0].usUsage = 0x05;
    devices[0].dwFlags = RIDEV_DEVNOTIFY;
    devices[0].hwndTarget = message_window;
    // joystick devices
    devices[1].usUsagePage = 0x01;
    devices[1].usUsage = 0x04;
    devices[1].dwFlags = RIDEV_DEVNOTIFY;
    devices[1].hwndTarget = message_window;

    if (!RegisterRawInputDevices(devices.data(), static_cast<UINT>(devices.size()),
                                 static_cast<UINT>(sizeof(decltype(devices)::value_type))))
    {
      ERROR_LOG_FMT(CONTROLLERINTERFACE, "RegisterRawInputDevices failed: {}", GetLastError());
      return;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (msg.message == WM_DOLPHIN_STOP)
        break;
    }
  });

  s_message_window = message_window_promise.get_future().get();
}

void ciface::Win32::PopulateDevices(void* hwnd)
{
  if (s_thread.joinable())
  {
    s_hwnd = static_cast<HWND>(hwnd);
    // To avoid blocking this thread until the async population has finished, directly do it here
    // (we need the DInput Keyboard and Mouse "default" device to always be added without any wait).
    std::lock_guard lk_population(s_populate_mutex);
    s_first_populate_devices_asked.Set();
    ciface::DInput::PopulateDevices(s_hwnd);
    ciface::XInput::PopulateDevices();
  }
  else
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE,
                  "win32 asked to populate devices, but device thread isn't running");
  }
}

void ciface::Win32::ChangeWindow(void* hwnd)
{
  if (s_thread.joinable())  // "Has init?"
  {
    s_hwnd = static_cast<HWND>(hwnd);
    std::lock_guard lk_population(s_populate_mutex);
    ciface::DInput::ChangeWindow(s_hwnd);
  }
}

void ciface::Win32::DeInit()
{
  NOTICE_LOG_FMT(CONTROLLERINTERFACE, "win32 DeInit");
  if (s_thread.joinable())
  {
    PostMessage(s_message_window, WM_DOLPHIN_STOP, 0, 0);
    s_thread.join();
    s_message_window = nullptr;
    s_first_populate_devices_asked.Clear();
    DInput::DeInit();
  }
  s_hwnd = nullptr;

  XInput::DeInit();
}
