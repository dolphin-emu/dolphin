// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include <algorithm>

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
#ifdef CIFACE_USE_STEAMDECK
#include "InputCommon/ControllerInterface/SteamDeck/SteamDeck.h"
#endif

ControllerInterface g_controller_interface;

// We need to save which input channel we are in by thread, so we can access the correct input
// update values in different threads by input channel. We start from InputChannel::Host on all
// threads as hotkeys are updated from a worker thread, but UI can read from the main thread. This
// will never interfere with game threads.
static thread_local ciface::InputChannel tls_input_channel = ciface::InputChannel::Host;

static thread_local bool tls_is_updating_devices = false;

void ControllerInterface::Initialize(const WindowSystemInfo& wsi)
{
  if (m_is_init)
    return;

  std::lock_guard lk_population(m_devices_population_mutex);

  m_wsi = wsi;

  m_populating_devices_counter = 1;

#ifdef CIFACE_USE_WIN32
  ciface::Win32::Init(wsi.render_window);
#endif
#ifdef CIFACE_USE_XLIB
// nothing needed
#endif
#ifdef CIFACE_USE_OSX
// nothing needed for Quartz
#endif
#ifdef CIFACE_USE_SDL
  m_input_backends.emplace_back(ciface::SDL::CreateInputBackend(this));
#endif
#ifdef CIFACE_USE_ANDROID
  ciface::Android::Init();
#endif
#ifdef CIFACE_USE_EVDEV
  m_input_backends.emplace_back(ciface::evdev::CreateInputBackend(this));
#endif
#ifdef CIFACE_USE_PIPES
// nothing needed
#endif
#ifdef CIFACE_USE_DUALSHOCKUDPCLIENT
  m_input_backends.emplace_back(ciface::DualShockUDPClient::CreateInputBackend(this));
#endif
#ifdef CIFACE_USE_STEAMDECK
  m_input_backends.emplace_back(ciface::SteamDeck::CreateInputBackend(this));
#endif

  // Don't allow backends to add devices before the first RefreshDevices() as they will be cleaned
  // there. Or they'd end up waiting on the devices mutex if populated from another thread.
  m_is_init = true;

  RefreshDevices();

  // Devices writes are already protected by m_devices_population_mutex but this won't hurt
  m_devices_mutex.lock();
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

  // We lock m_devices_population_mutex here to make everything simpler.
  // Multiple devices classes have their own "hotplug" thread, and can add/remove devices at any
  // time, while actual writes to "m_devices" are safe, the order in which they happen is not. That
  // means a thread could be adding devices while we are removing them from a different thread,
  // or removing them as we are populating them (causing missing or duplicate devices).
  std::lock_guard lk_population(m_devices_population_mutex);

#if defined(CIFACE_USE_WIN32) && !defined(CIFACE_USE_XLIB) && !defined(CIFACE_USE_OSX)
  // If only the window changed, avoid removing and re-adding all devices.
  // Instead only refresh devices that require the window handle.
  if (reason == RefreshReason::WindowChangeOnly)
  {
    m_populating_devices_counter.fetch_add(1);

    // No need to do anything else in this case.
    // Only (Win32) DInput needs the window handle to be updated.
    ciface::Win32::ChangeWindow(m_wsi.render_window);

    if (m_populating_devices_counter.fetch_sub(1) == 1)
      InvokeDevicesChangedCallbacks();
    return;
  }
#endif

  m_populating_devices_counter.fetch_add(1);

  // Make sure shared_ptr<Device> objects are released before repopulating.
  ClearDevices();

  // Some of these calls won't immediately populate devices, but will do it async
  // with their own PlatformPopulateDevices().
  // This means that devices might end up in different order, unless we override their priority.
  // It also means they might appear as "disconnected" in the Qt UI for a tiny bit of time.
  // This helps the emulation and host thread to not stall when repopulating devices for any reason.
  // Every platform that adds a device that is meant to be used as default device should try to not
  // do it async, to not risk the emulated controllers default config loading not finding a default
  // device.

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
    ciface::Quartz::PopulateDevices(m_wsi.render_window);
  }
#endif
#ifdef CIFACE_USE_ANDROID
  ciface::Android::PopulateDevices();
#endif
#ifdef CIFACE_USE_PIPES
  ciface::Pipes::PopulateDevices();
#endif

  for (auto& backend : m_input_backends)
    backend->PopulateDevices();

  WiimoteReal::PopulateDevices();

  if (m_populating_devices_counter.fetch_sub(1) == 1)
    InvokeDevicesChangedCallbacks();
}

void ControllerInterface::PlatformPopulateDevices(std::function<void()> callback)
{
  if (!m_is_init)
    return;

  std::lock_guard lk_population(m_devices_population_mutex);

  m_populating_devices_counter.fetch_add(1);

  callback();

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
  ciface::Quartz::DeInit();
#endif
#ifdef CIFACE_USE_ANDROID
  ciface::Android::Shutdown();
#endif

  // Empty the container of input backends to deconstruct and deinitialize them.
  m_input_backends.clear();

  // Make sure no devices had been added within Shutdown() in the time
  // between checking they checked atomic m_is_init bool and we changed it.
  // We couldn't have locked m_devices_population_mutex for the whole Shutdown()
  // as it could cause deadlocks. Note that this is still not 100% safe as some backends are
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

    for (const auto& d : m_devices)
    {
      // Set outputs to ZERO before destroying device
      for (ciface::Core::Device::Output* o : d->Outputs())
        o->SetState(0);
    }

    // Devices could still be alive after this as there might be shared ptrs around holding them.
    // The InvokeDevicesChangedCallbacks() underneath should always clean all of them (it needs to).
    m_devices.clear();
  }

  InvokeDevicesChangedCallbacks();
}

bool ControllerInterface::AddDevice(std::shared_ptr<ciface::Core::Device> device)
{
  // If we are shutdown (or in process of shutting down) ignore this request:
  if (!m_is_init)
    return false;

  ASSERT_MSG(CONTROLLERINTERFACE, !tls_is_updating_devices,
             "Devices shouldn't be added within input update calls, there is a risk of deadlock "
             "if another thread was already here");

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

  ASSERT_MSG(CONTROLLERINTERFACE, !tls_is_updating_devices,
             "Devices shouldn't be removed within input update calls, there is a risk of deadlock "
             "if another thread was already here");

  std::lock_guard lk_population(m_devices_population_mutex);

  bool any_removed;
  {
    std::lock_guard lk(m_devices_mutex);
    auto it = std::remove_if(m_devices.begin(), m_devices.end(), [&callback](const auto& dev) {
      if (callback(dev.get()))
      {
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
void ControllerInterface::UpdateInput()
{
  // This should never happen
  ASSERT(m_is_init);
  if (!m_is_init)
    return;

  // We add the devices to remove while we still have the "m_devices_mutex" locked.
  // This guarantees that:
  // -We won't try to lock "m_devices_population_mutex" while it was already locked and waiting
  //  for "m_devices_mutex", which would result in dead lock.
  // -We don't keep shared ptrs on devices and thus unwillingly keep them alive even if somebody
  //  is currently trying to remove them (and needs them destroyed on the spot).
  // -If somebody else destroyed them in the meantime, we'll know which ones have been destroyed.
  std::vector<std::weak_ptr<ciface::Core::Device>> devices_to_remove;

  {
    // TODO: if we are an emulation input channel, we should probably always lock.
    // Prefer outdated values over blocking UI or CPU thread (this avoids short but noticeable frame
    // drops)
    if (!m_devices_mutex.try_lock())
      return;

    std::lock_guard lk_devices(m_devices_mutex, std::adopt_lock);

    tls_is_updating_devices = true;

    for (auto& backend : m_input_backends)
      backend->UpdateInput(devices_to_remove);

    for (const auto& d : m_devices)
    {
      // Theoretically we could avoid updating input on devices that don't have any references to
      // them, but in practice a few devices types could break in different ways, so we don't
      if (d->UpdateInput() == ciface::Core::DeviceRemoval::Remove)
        devices_to_remove.push_back(d);
    }

    tls_is_updating_devices = false;
  }

  if (devices_to_remove.size() > 0)
  {
    RemoveDevice([&](const ciface::Core::Device* device) {
      return std::any_of(devices_to_remove.begin(), devices_to_remove.end(),
                         [device](const std::weak_ptr<ciface::Core::Device>& d) {
                           return d.lock().get() == device;
                         });
    });
  }
}

void ControllerInterface::SetCurrentInputChannel(ciface::InputChannel input_channel)
{
  tls_input_channel = input_channel;
}

ciface::InputChannel ControllerInterface::GetCurrentInputChannel()
{
  return tls_input_channel;
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

void ControllerInterface::SetMouseCenteringRequested(bool center)
{
  m_requested_mouse_centering = center;
}

bool ControllerInterface::IsMouseCenteringRequested() const
{
  return m_requested_mouse_centering.load();
}

// Register a callback to be called when a device is added or removed (as from the input backends'
// hotplug thread), or when devices are refreshed
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
