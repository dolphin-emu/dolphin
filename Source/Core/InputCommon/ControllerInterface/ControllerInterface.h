// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <mutex>

#include "Common/Matrix.h"
#include "Common/WindowSystemInfo.h"
#include "InputCommon/ControllerInterface/Device.h"

// enable disable sources
#ifdef _WIN32
#define CIFACE_USE_WIN32
#endif
#if defined(HAVE_X11) && HAVE_X11
#define CIFACE_USE_XLIB
#endif
#if defined(MACOS)
#define CIFACE_USE_OSX
#endif
#if defined(HAVE_LIBEVDEV) && defined(HAVE_LIBUDEV)
#define CIFACE_USE_EVDEV
#endif
#if defined(USE_PIPES)
#define CIFACE_USE_PIPES
#endif
#define CIFACE_USE_DUALSHOCKUDPCLIENT

//
// ControllerInterface
//
// Some crazy shit I made to control different device inputs and outputs
// from lots of different sources, hopefully more easily.
//
class ControllerInterface : public ciface::Core::DeviceContainer
{
public:
  using HotplugCallbackHandle = std::list<std::function<void()>>::iterator;

  ControllerInterface() : m_is_init(false) {}
  void Initialize(const WindowSystemInfo& wsi);
  void ChangeWindow(void* hwnd);
  void RefreshDevices();
  void Shutdown();
  void AddDevice(std::shared_ptr<ciface::Core::Device> device);
  void RemoveDevice(std::function<bool(const ciface::Core::Device*)> callback);
  bool IsInit() const { return m_is_init; }
  void UpdateInput();

  // Set adjustment from the full render window aspect-ratio to the drawn aspect-ratio.
  // Used to fit mouse cursor inputs to the relevant region of the render window.
  void SetAspectRatioAdjustment(float);

  // Calculated from the aspect-ratio adjustment.
  // Inputs based on window coordinates should be multiplied by this.
  Common::Vec2 GetWindowInputScale() const;

  HotplugCallbackHandle RegisterDevicesChangedCallback(std::function<void(void)> callback);
  void UnregisterDevicesChangedCallback(const HotplugCallbackHandle& handle);
  void InvokeDevicesChangedCallbacks() const;

private:
  std::list<std::function<void()>> m_devices_changed_callbacks;
  mutable std::mutex m_callbacks_mutex;
  std::atomic<bool> m_is_init;
  std::atomic<bool> m_is_populating_devices{false};
  WindowSystemInfo m_wsi;
  std::atomic<float> m_aspect_ratio_adjustment = 1;
};

extern ControllerInterface g_controller_interface;
