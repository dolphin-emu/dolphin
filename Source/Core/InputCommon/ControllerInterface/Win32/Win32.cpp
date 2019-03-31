// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/Win32/Win32.h"

#include <windows.h>

#include <array>
#include <future>
#include <thread>

#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "InputCommon/ControllerInterface/DInput/DInput.h"
#include "InputCommon/ControllerInterface/XInput/XInput.h"

constexpr UINT WM_DOLPHIN_STOP = WM_USER;

static Common::Event s_done_populating;
static std::atomic<HWND> s_hwnd;
static HWND s_message_window;
static std::thread s_thread;

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  if (message == WM_INPUT_DEVICE_CHANGE)
  {
    ciface::DInput::PopulateDevices(s_hwnd);
    ciface::XInput::PopulateDevices();
    s_done_populating.Set();
  }

  return DefWindowProc(hwnd, message, wparam, lparam);
}

void ciface::Win32::Init(void* hwnd)
{
  s_hwnd = static_cast<HWND>(hwnd);
  XInput::Init();

  std::promise<HWND> message_window_promise;

  s_thread = std::thread([&message_window_promise] {
    HWND message_window = nullptr;
    Common::ScopeGuard promise_guard([&] { message_window_promise.set_value(message_window); });

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    {
      ERROR_LOG(SERIALINTERFACE, "CoInitializeEx failed: %i", GetLastError());
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
      NOTICE_LOG(SERIALINTERFACE, "RegisterClassEx failed: %i", GetLastError());
      return;
    }
    Common::ScopeGuard unregister([&window_class] {
      if (!UnregisterClass(MAKEINTATOM(window_class), GetModuleHandle(nullptr)))
        ERROR_LOG(SERIALINTERFACE, "UnregisterClass failed: %i", GetLastError());
    });

    message_window = CreateWindowEx(0, L"Message", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                    nullptr, nullptr);
    promise_guard.Exit();
    if (!message_window)
    {
      ERROR_LOG(SERIALINTERFACE, "CreateWindowEx failed: %i", GetLastError());
      return;
    }
    Common::ScopeGuard destroy([&] {
      if (!DestroyWindow(message_window))
        ERROR_LOG(SERIALINTERFACE, "DestroyWindow failed: %i", GetLastError());
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
      ERROR_LOG(SERIALINTERFACE, "RegisterRawInputDevices failed: %i", GetLastError());
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
    s_done_populating.Reset();
    PostMessage(s_message_window, WM_INPUT_DEVICE_CHANGE, 0, 0);
    if (!s_done_populating.WaitFor(std::chrono::seconds(10)))
      ERROR_LOG(SERIALINTERFACE, "win32 timed out when trying to populate devices");
  }
  else
  {
    ERROR_LOG(SERIALINTERFACE, "win32 asked to populate devices, but device thread isn't running");
  }
}

void ciface::Win32::DeInit()
{
  NOTICE_LOG(SERIALINTERFACE, "win32 DeInit");
  if (s_thread.joinable())
  {
    PostMessage(s_message_window, WM_DOLPHIN_STOP, 0, 0);
    s_thread.join();
    s_message_window = nullptr;
  }

  XInput::DeInit();
}
