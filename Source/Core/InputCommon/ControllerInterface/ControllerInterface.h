// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <mutex>

#include "Common/Matrix.h"
#include "Common/WindowSystemInfo.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/InputBackend.h"

// enable disable sources
#ifdef _WIN32
#define CIFACE_USE_WIN32
#endif
#ifdef HAVE_X11
#define CIFACE_USE_XLIB
#endif
#if defined(__APPLE__)
#define CIFACE_USE_OSX
#endif
#if defined(HAVE_LIBEVDEV) && defined(HAVE_LIBUDEV)
#define CIFACE_USE_EVDEV
#endif
#if defined(USE_PIPES)
#define CIFACE_USE_PIPES
#endif
#define CIFACE_USE_DUALSHOCKUDPCLIENT
#if defined(HAVE_SDL2)
#define CIFACE_USE_SDL
#endif
#if defined(HAVE_HIDAPI)
#define CIFACE_USE_STEAMDECK
#endif

namespace ciface
{
// A thread local "input channel" is maintained to handle the state of relative inputs.
// This allows simultaneous use of relative inputs across different input contexts.
// e.g. binding relative mouse movements to both GameCube controllers and FreeLook.
// These operate at different rates and processing one would break the other without separate state.
enum class InputChannel
{
  Host,
  SerialInterface,
  Bluetooth,
  FreeLook,
  Count,
};

}  // namespace ciface

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

  enum class WindowChangeReason
  {
    // Application is shutting down
    Exit,
    Other
  };

  enum class RefreshReason
  {
    // Only the window changed.
    WindowChangeOnly,
    // User requested, or any other internal reason (e.g. init).
    // The window might have changed anyway.
    Other
  };

  ControllerInterface() : m_is_init(false) {}
  void Initialize(const WindowSystemInfo& wsi);
  // Only call from one thread at a time.
  void ChangeWindow(void* hwnd, WindowChangeReason reason = WindowChangeReason::Other);
  // Can be called by any thread at any time (when initialized).
  void RefreshDevices(RefreshReason reason = RefreshReason::Other);
  void Shutdown();
  bool AddDevice(std::shared_ptr<ciface::Core::Device> device);
  // Removes all the devices the function returns true to.
  // If all the devices shared ptrs need to be destroyed immediately,
  // set force_devices_release to true.
  void RemoveDevice(std::function<bool(const ciface::Core::Device*)> callback,
                    bool force_devices_release = false);
  // This is mandatory to use on device populations functions that can be called concurrently by
  // more than one thread, or that are called by a single other thread.
  // Without this, our devices list might end up in a mixed state.
  void PlatformPopulateDevices(std::function<void()> callback);
  bool IsInit() const { return m_is_init; }
  void UpdateInput();

  // Set adjustment from the full render window aspect-ratio to the drawn aspect-ratio.
  // Used to fit mouse cursor inputs to the relevant region of the render window.
  void SetAspectRatioAdjustment(float);

  // Calculated from the aspect-ratio adjustment.
  // Inputs based on window coordinates should be multiplied by this.
  Common::Vec2 GetWindowInputScale() const;

  // Request that the mouse cursor should be centered in the render window at the next opportunity.
  void SetMouseCenteringRequested(bool center);

  bool IsMouseCenteringRequested() const;

  HotplugCallbackHandle RegisterDevicesChangedCallback(std::function<void(void)> callback);
  void UnregisterDevicesChangedCallback(const HotplugCallbackHandle& handle);
  void InvokeDevicesChangedCallbacks() const;

  static void SetCurrentInputChannel(ciface::InputChannel);
  static ciface::InputChannel GetCurrentInputChannel();

private:
  void ClearDevices();

  std::list<std::function<void()>> m_devices_changed_callbacks;
  mutable std::recursive_mutex m_devices_population_mutex;
  mutable std::mutex m_callbacks_mutex;
  std::atomic<bool> m_is_init;
  // This is now always protected by m_devices_population_mutex, so
  // it doesn't really need to be a counter or atomic anymore (it could be a raw bool),
  // but we keep it so for simplicity, in case we changed the design.
  std::atomic<int> m_populating_devices_counter;
  WindowSystemInfo m_wsi;
  std::atomic<float> m_aspect_ratio_adjustment = 1;
  std::atomic<bool> m_requested_mouse_centering = false;

  std::vector<std::unique_ptr<ciface::InputBackend>> m_input_backends;
};

namespace ciface
{
template <typename T>
class RelativeInputState
{
public:
  void Update()
  {
    const auto channel = int(ControllerInterface::GetCurrentInputChannel());

    m_value[channel] = m_delta[channel];
    m_delta[channel] = {};
  }

  T GetValue() const
  {
    const auto channel = int(ControllerInterface::GetCurrentInputChannel());

    return m_value[channel];
  }

  void Move(T delta)
  {
    for (auto& d : m_delta)
      d += delta;
  }

private:
  std::array<T, int(InputChannel::Count)> m_value;
  std::array<T, int(InputChannel::Count)> m_delta;
};
}  // namespace ciface

extern ControllerInterface g_controller_interface;
