// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include <algorithm>
#include <chrono>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

#ifdef CIFACE_USE_WIN32
#include "InputCommon/ControllerInterface/Win32/Win32.h"
#endif
#ifdef CIFACE_USE_XLIB
#include "InputCommon/ControllerInterface/Xlib/XInput2.h"
#endif
#ifdef CIFACE_USE_OSX
#include "InputCommon/ControllerInterface/OSX/OSX.h"
#include "InputCommon/ControllerInterface/Quartz/Quartz.h"
#endif
#ifdef CIFACE_USE_SDL
#include "InputCommon/ControllerInterface/SDL/SDL.h"
#endif
#ifdef CIFACE_USE_ANDROID
#include "InputCommon/ControllerInterface/Android/Android.h"
#endif
#ifdef CIFACE_USE_EVDEV
#include "InputCommon/ControllerInterface/evdev/evdev.h"
#endif
#ifdef CIFACE_USE_PIPES
#include "InputCommon/ControllerInterface/Pipes/Pipes.h"
#endif
#ifdef CIFACE_USE_DUALSHOCKUDPCLIENT
#include "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.h"
#endif

ControllerInterface g_controller_interface;

using Clock = std::chrono::steady_clock;

// We need to save which input channel we are in by thread, so we can access the correct input
// update values in different threads by input channel. We start from InputChannel::Host on all
// threads as hotkeys are updated from a worker thread, but UI can read from the main thread. This
// will never interfere with game threads.
static thread_local ciface::InputChannel tls_input_channel = ciface::InputChannel::Host;
static double s_input_channels_delta_seconds[u8(ciface::InputChannel::Max)];
static double s_input_channels_target_delta_seconds[u8(ciface::InputChannel::Max)];
static s32 s_input_channels_updates_per_target[u8(ciface::InputChannel::Max)];
static double s_input_channels_real_delta_seconds[u8(ciface::InputChannel::Max)];
static Clock::time_point s_input_channels_last_update[u8(ciface::InputChannel::Max)];

void ControllerInterface::Initialize(const WindowSystemInfo& wsi)
{
  if (m_is_init)
    return;

  for (u8 i = 0; i < u8(ciface::InputChannel::Max); ++i)
    s_input_channels_last_update[i] = Clock::now();

  std::lock_guard lk_population(m_devices_population_mutex);

  m_wsi = wsi;

  m_populating_devices_counter = 1;

  m_devices_mutex.lock();

#ifdef CIFACE_USE_WIN32
  ciface::Win32::Init(wsi.render_window);
#endif
#ifdef CIFACE_USE_XLIB
// nothing needed
#endif
#ifdef CIFACE_USE_OSX
// nothing needed for OSX and Quartz
#endif
#ifdef CIFACE_USE_SDL
  ciface::SDL::Init();
#endif
#ifdef CIFACE_USE_ANDROID
// nothing needed
#endif
#ifdef CIFACE_USE_EVDEV
  ciface::evdev::Init();
#endif
#ifdef CIFACE_USE_PIPES
// nothing needed
#endif
#ifdef CIFACE_USE_DUALSHOCKUDPCLIENT
  ciface::DualShockUDPClient::Init();
#endif

  // Don't allow backends to add devices before the first RefreshDevices() as they will be cleaned
  // there. Or they'd end up waiting on the devices mutex if populated from another thread.
  m_is_init = true;

  RefreshDevices();

  const bool devices_empty = m_devices.empty();

  m_devices_mutex.unlock();

  if (m_populating_devices_counter.fetch_sub(1) == 1 && !devices_empty)
    InvokeDevicesChangedCallbacks();
}

void ControllerInterface::ChangeWindow(void* hwnd, WindowChangeReason reason)
{
  if (!m_is_init)
    return;

  // This shouldn't use render_surface so no need to update it.
  m_wsi.render_window = hwnd;

  // No need to re-add devices if this is an application exit request
  if (reason == WindowChangeReason::Exit)
    ClearDevices();
  else
    RefreshDevices(RefreshReason::WindowChangeOnly);
}

void ControllerInterface::RefreshDevices(RefreshReason reason)
{
  if (!m_is_init)
    return;

#ifdef CIFACE_USE_OSX
  if (m_wsi.type == WindowSystemType::MacOS)
  {
    std::lock_guard lk_pre_population(m_pre_population_mutex);
    // This is needed to stop its threads before locking our mutexes, to avoid deadlocks
    // (in case it tried to add a device after we had locked m_devices_population_mutex).
    // There doesn't seem to be an easy to way to repopulate OSX devices without restarting
    // its hotplug thread. This will not release its devices, that's still done below.
    ciface::OSX::DeInit();
  }
#endif

  // This lock has two main functions:
  // -Avoid a deadlock between m_devices_mutex and ControllerEmu::s_state_mutex when
  // InvokeDevicesChangedCallbacks() is called concurrently by two different threads.
  // -Avoid devices being destroyed while others of the same type are being created.
  // This wasn't thread safe in multiple device sources.
  std::lock_guard lk_population(m_devices_population_mutex);

#if defined(CIFACE_USE_WIN32) && !defined(CIFACE_USE_XLIB) && !defined(CIFACE_USE_OSX)
  // If only the window changed, avoid removing and re-adding all devices.
  // Instead only refresh devices that require the window handle.
  if (reason == RefreshReason::WindowChangeOnly)
  {
    m_populating_devices_counter.fetch_add(1);

    {
      std::lock_guard lk(m_devices_mutex);
      // No need to do anything else in this case.
      // Only (Win32) DInput needs the window handle to be updated.
      ciface::Win32::ChangeWindow(m_wsi.render_window);
    }

    if (m_populating_devices_counter.fetch_sub(1) == 1)
      InvokeDevicesChangedCallbacks();
    return;
  }
#endif

  m_populating_devices_counter.fetch_add(1);

  // We lock m_devices_mutex here to make everything simpler.
  // Multiple devices classes have their own "hotplug" thread, and can add/remove devices at any
  // time, while actual writes to "m_devices" are safe, the order in which they happen is not. That
  // means a thread could be adding devices while we are removing them, or removing them as we are
  // populating them (causing missing or duplicate devices).
  m_devices_mutex.lock();

  // Make sure shared_ptr<Device> objects are released before repopulating.
  ClearDevices();

  // Some of these calls won't immediately populate devices, but will do it async
  // with their own PlatformPopulateDevices().
  // This means that devices might end up in different order, unless we override their priority.
  // It also means they might appear as "disconnected" in the Qt UI for a tiny bit of time.
  // There is an extremely low chance of no devices being added before emulated controllers load
  // their default settings, but at worse that would cause a controller to have no default device.

#ifdef CIFACE_USE_WIN32
  ciface::Win32::PopulateDevices(m_wsi.render_window);
#endif
#ifdef CIFACE_USE_XLIB
  if (m_wsi.type == WindowSystemType::X11)
    ciface::XInput2::PopulateDevices(m_wsi.render_window);
#endif
#ifdef CIFACE_USE_OSX
  if (m_wsi.type == WindowSystemType::MacOS)
  {
    {
      std::lock_guard lk_pre_population(m_pre_population_mutex);
      ciface::OSX::Init();
    }
    ciface::Quartz::PopulateDevices(m_wsi.render_window);
  }
#endif
#ifdef CIFACE_USE_SDL
  ciface::SDL::PopulateDevices();
#endif
#ifdef CIFACE_USE_ANDROID
  ciface::Android::PopulateDevices();
#endif
#ifdef CIFACE_USE_EVDEV
  ciface::evdev::PopulateDevices();
#endif
#ifdef CIFACE_USE_PIPES
  ciface::Pipes::PopulateDevices();
#endif
#ifdef CIFACE_USE_DUALSHOCKUDPCLIENT
  ciface::DualShockUDPClient::PopulateDevices();
#endif

  WiimoteReal::PopulateDevices();

  m_devices_mutex.unlock();

  if (m_populating_devices_counter.fetch_sub(1) == 1)
    InvokeDevicesChangedCallbacks();
}

void ControllerInterface::PlatformPopulateDevices(std::function<void()> callback)
{
  if (!m_is_init)
    return;

  std::lock_guard lk_population(m_devices_population_mutex);

  m_populating_devices_counter.fetch_add(1);

  {
    std::lock_guard lk(m_devices_mutex);

    callback();
  }

  if (m_populating_devices_counter.fetch_sub(1) == 1)
    InvokeDevicesChangedCallbacks();
}

// Remove all devices and call library cleanup functions
void ControllerInterface::Shutdown()
{
  if (!m_is_init)
    return;

  // Prevent additional devices from being added during shutdown.
  m_is_init = false;
  // Additional safety measure to avoid InvokeDevicesChangedCallbacks()
  m_populating_devices_counter = 1;

  // Update control references so shared_ptr<Device>s are freed up BEFORE we shutdown the backends.
  ClearDevices();

#ifdef CIFACE_USE_WIN32
  ciface::Win32::DeInit();
#endif
#ifdef CIFACE_USE_XLIB
// nothing needed
#endif
#ifdef CIFACE_USE_OSX
  ciface::OSX::DeInit();
  ciface::Quartz::DeInit();
#endif
#ifdef CIFACE_USE_SDL
  ciface::SDL::DeInit();
#endif
#ifdef CIFACE_USE_ANDROID
// nothing needed
#endif
#ifdef CIFACE_USE_EVDEV
  ciface::evdev::Shutdown();
#endif
#ifdef CIFACE_USE_DUALSHOCKUDPCLIENT
  ciface::DualShockUDPClient::DeInit();
#endif

  // Make sure no devices had been added within Shutdown() in the time
  // between checking they checked atomic m_is_init bool and we changed it.
  // We couldn't have locked m_devices_mutex nor m_devices_population_mutex for the whole Shutdown()
  // as they could cause deadlocks. Note that this is still not 100% safe as some backends are
  // shut down in other places, possibly adding devices after we have shut down, but the chances of
  // that happening are basically zero.
  ClearDevices();
}

void ControllerInterface::ClearDevices()
{
  std::lock_guard lk_population(m_devices_population_mutex);

  {
    std::lock_guard lk(m_devices_mutex);

    if (m_devices.empty())
      return;

    // Set outputs to ZERO before destroying device, just to be safe.
    for (const auto& d : m_devices)
      d->ResetOutput();

    // Devices will still be alive after this: there are shared ptrs around the code holding them,
    // but InvokeDevicesChangedCallbacks() will clean all of them.
    m_devices.clear();
  }

  InvokeDevicesChangedCallbacks();
}

bool ControllerInterface::AddDevice(std::shared_ptr<ciface::Core::Device> device)
{
  // If we are shutdown (or in process of shutting down) ignore this request:
  if (!m_is_init)
    return false;

  std::lock_guard lk_population(m_devices_population_mutex);

  {
    std::lock_guard lk(m_devices_mutex);

    const auto is_id_in_use = [&device, this](int id) {
      return std::any_of(m_devices.begin(), m_devices.end(), [&device, &id](const auto& d) {
        return d->GetSource() == device->GetSource() && d->GetName() == device->GetName() &&
               d->GetId() == id;
      });
    };

    const auto preferred_id = device->GetPreferredId();
    if (preferred_id.has_value() && !is_id_in_use(*preferred_id))
    {
      // Use the device's preferred ID if available.
      device->SetId(*preferred_id);
    }
    else
    {
      // Find the first available ID to use.
      int id = 0;
      while (is_id_in_use(id))
        ++id;

      device->SetId(id);
    }

    NOTICE_LOG_FMT(CONTROLLERINTERFACE, "Added device: {}", device->GetQualifiedName());

    m_devices.emplace_back(std::move(device));

    // We can't (and don't want) to control the order in which devices are added, but we
    // need their order to be consistent, and we need the same one to always be the first, where
    // present (the keyboard and mouse device usually). This is because when defaulting a
    // controller profile, it will automatically select the first device in the list as its default.
    std::stable_sort(m_devices.begin(), m_devices.end(),
                     [](const std::shared_ptr<ciface::Core::Device>& a,
                        const std::shared_ptr<ciface::Core::Device>& b) {
                       // It would be nice to sort devices by Source then Name then ID but it's
                       // better to leave them sorted by the add order, which also avoids breaking
                       // the order on other platforms that are less tested.
                       return a->GetSortPriority() > b->GetSortPriority();
                     });
  }

  if (!m_populating_devices_counter)
    InvokeDevicesChangedCallbacks();
  return true;
}

void ControllerInterface::RemoveDevice(std::function<bool(const ciface::Core::Device*)> callback,
                                       bool force_devices_release)
{
  // If we are shutdown (or in process of shutting down) ignore this request:
  if (!m_is_init)
    return;

  std::lock_guard lk_population(m_devices_population_mutex);

  bool any_removed;
  {
    std::lock_guard lk(m_devices_mutex);
    auto it = std::remove_if(m_devices.begin(), m_devices.end(), [&callback](const auto& dev) {
      if (callback(dev.get()))
      {
        dev->ResetOutput();
        NOTICE_LOG_FMT(CONTROLLERINTERFACE, "Removed device: {}", dev->GetQualifiedName());
        return true;
      }
      return false;
    });
    const size_t prev_size = m_devices.size();
    m_devices.erase(it, m_devices.end());
    any_removed = m_devices.size() != prev_size;
  }

  if (any_removed && (!m_populating_devices_counter || force_devices_release))
    InvokeDevicesChangedCallbacks();
}

// Update input for all devices if lock can be acquired without waiting.
void ControllerInterface::UpdateInput(ciface::InputChannel input_channel, double delta_seconds,
                                      double target_delta_seconds, u32 updates_per_target)
{
  // This should never happen
  ASSERT(m_is_init);
  if (!m_is_init)
    return;

  // We set the read/write input channel here, see Device::RelativeInput for more info.
  // The other information is used by FunctionExpression(s) to determine their timings.
  // Inputs for this channel will be read immediately after this call.
  // Make sure to not read them after the input channel has changed again (on the same thread).
  tls_input_channel = input_channel;
  u8 i = u8(tls_input_channel);
  // This is not the actual world elapsed time, it's the emulation elapsed time
  s_input_channels_delta_seconds[i] = delta_seconds;
  // Delta seconds can be bigger or smaller than the target one, but they should average out
  s_input_channels_target_delta_seconds[i] =
      target_delta_seconds > 0.f ? target_delta_seconds : delta_seconds;
  s_input_channels_updates_per_target[i] = updates_per_target;

  // Calculate the real/world elapsed time.
  // Useful to turn relative axes into "rate of change"/speed values usable by games
  const auto now = Clock::now();
  Clock::time_point& input_channel_last_update = s_input_channels_last_update[i];
  s_input_channels_real_delta_seconds[i] =
      std::chrono::duration_cast<std::chrono::duration<double>>(now - input_channel_last_update)
          .count();
  input_channel_last_update = now;

  // TODO: if we are an emulation input channel, we should probably always lock
  // Prefer outdated values over blocking UI or CPU thread (avoids short but noticeable frame drop)
  if (m_devices_mutex.try_lock())
  {
    std::lock_guard lk(m_devices_mutex, std::adopt_lock);

    // Device::UpdateInput() would modify values read by ControlReference(s)
    const auto lock = ControllerEmu::EmulatedController::GetDevicesInputLock();

    for (const auto& d : m_devices)
    {
      // Theoretically we could avoid updating input on devices that don't have any references to
      // them, but in practice a few devices types could break in different ways, so we don't
      d->UpdateInput();
    }
  }
}

void ControllerInterface::SetInputChannel(ciface::InputChannel input_channel)
{
  tls_input_channel = input_channel;
}

// Call this when you are toggling pause or "closing" (stopping to update) an input channel.
// Not mandatory.
void ControllerInterface::SetChannelRunning(ciface::InputChannel input_channel, bool running)
{
  if (!m_is_init)
    return;

  std::lock_guard lk(m_devices_mutex);

  const ciface::InputChannel prev_input_channel = tls_input_channel;
  tls_input_channel = input_channel;

  if (running)
  {
    // No need to clean s_input_channels_delta_seconds and the others
    s_input_channels_last_update[u8(tls_input_channel)] = Clock::now();

    const auto lock = ControllerEmu::EmulatedController::GetDevicesInputLock();

    for (const auto& d : m_devices)
      d->ResetInput();
  }
  else
  {
    for (const auto& d : m_devices)
    {
      // This isn't 100% right as other input channels could still be changing the outputs but
      // as of now that could never happen and even so, it's still better than stuck output values
      d->ResetOutput();
    }
  }

  tls_input_channel = prev_input_channel;
}

void ControllerInterface::SetAspectRatioAdjustment(float value)
{
  m_aspect_ratio_adjustment = value;
}

Common::Vec2 ControllerInterface::GetWindowInputScale() const
{
  const auto ar = m_aspect_ratio_adjustment.load();

  if (ar > 1)
    return {1.f, ar};
  else
    return {1 / ar, 1.f};
}

// Register a callback to be called when a device is added or removed (as from the input backends'
// hotplug thread), or when devices are refreshed.
// Returns a handle for later removing the callback.
ControllerInterface::HotplugCallbackHandle
ControllerInterface::RegisterDevicesChangedCallback(std::function<void()> callback)
{
  std::lock_guard lk(m_callbacks_mutex);
  m_devices_changed_callbacks.emplace_back(std::move(callback));
  return std::prev(m_devices_changed_callbacks.end());
}

// Unregister a device callback.
void ControllerInterface::UnregisterDevicesChangedCallback(const HotplugCallbackHandle& handle)
{
  std::lock_guard lk(m_callbacks_mutex);
  m_devices_changed_callbacks.erase(handle);
}

// Invoke all callbacks that were registered
void ControllerInterface::InvokeDevicesChangedCallbacks() const
{
  m_callbacks_mutex.lock();
  const auto devices_changed_callbacks = m_devices_changed_callbacks;
  m_callbacks_mutex.unlock();
  for (const auto& callback : devices_changed_callbacks)
    callback();
}

ciface::InputChannel ControllerInterface::GetCurrentInputChannel()
{
  return tls_input_channel;
}

double ControllerInterface::GetCurrentInputDeltaSeconds()
{
  return s_input_channels_delta_seconds[u8(tls_input_channel)];
}

double ControllerInterface::GetTargetInputDeltaSeconds()
{
  return s_input_channels_target_delta_seconds[u8(tls_input_channel)];
}

s32 ControllerInterface::GetInputUpdatesPerTarget()
{
  return s_input_channels_updates_per_target[u8(tls_input_channel)];
}

double ControllerInterface::GetCurrentRealInputDeltaSeconds()
{
  return s_input_channels_real_delta_seconds[u8(tls_input_channel)];
}
