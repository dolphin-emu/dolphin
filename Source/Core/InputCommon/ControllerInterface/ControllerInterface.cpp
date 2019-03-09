// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include <algorithm>

#include "Common/Logging/Log.h"

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

ControllerInterface g_controller_interface;

void ControllerInterface::Initialize(const WindowSystemInfo& wsi)
{
  if (m_is_init)
    return;

  m_wsi = wsi;

  // Allow backends to add devices as soon as they are initialized.
  m_is_init = true;

  m_is_populating_devices = true;

#ifdef CIFACE_USE_WIN32
  ciface::Win32::Init(wsi.render_surface);
#endif
#ifdef CIFACE_USE_XLIB
// nothing needed
#endif
#ifdef CIFACE_USE_OSX
  if (m_wsi.type == WindowSystemType::MacOS)
    ciface::OSX::Init(wsi.render_surface);
// nothing needed for Quartz
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

  RefreshDevices();
}

void ControllerInterface::ChangeWindow(void* hwnd)
{
  if (!m_is_init)
    return;

  m_wsi.render_surface = hwnd;
  RefreshDevices();
}

void ControllerInterface::RefreshDevices()
{
  if (!m_is_init)
    return;

  {
    std::lock_guard<std::mutex> lk(m_devices_mutex);
    m_devices.clear();
  }

  m_is_populating_devices = true;

  // Make sure shared_ptr<Device> objects are released before repopulating.
  InvokeDevicesChangedCallbacks();

#ifdef CIFACE_USE_WIN32
  ciface::Win32::PopulateDevices(m_wsi.render_surface);
#endif
#ifdef CIFACE_USE_XLIB
  if (m_wsi.type == WindowSystemType::X11)
    ciface::XInput2::PopulateDevices(m_wsi.render_surface);
#endif
#ifdef CIFACE_USE_OSX
  if (m_wsi.type == WindowSystemType::MacOS)
  {
    ciface::OSX::PopulateDevices(m_wsi.render_surface);
    ciface::Quartz::PopulateDevices(m_wsi.render_surface);
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

  m_is_populating_devices = false;
  InvokeDevicesChangedCallbacks();
}

// Remove all devices and call library cleanup functions
void ControllerInterface::Shutdown()
{
  if (!m_is_init)
    return;

  // Prevent additional devices from being added during shutdown.
  m_is_init = false;

  {
    std::lock_guard<std::mutex> lk(m_devices_mutex);

    for (const auto& d : m_devices)
    {
      // Set outputs to ZERO before destroying device
      for (ciface::Core::Device::Output* o : d->Outputs())
        o->SetState(0);
    }

    m_devices.clear();
  }

  // This will update control references so shared_ptr<Device>s are freed up
  // BEFORE we shutdown the backends.
  InvokeDevicesChangedCallbacks();

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
}

void ControllerInterface::AddDevice(std::shared_ptr<ciface::Core::Device> device)
{
  // If we are shutdown (or in process of shutting down) ignore this request:
  if (!m_is_init)
    return;

  {
    std::lock_guard<std::mutex> lk(m_devices_mutex);

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

    NOTICE_LOG(SERIALINTERFACE, "Added device: %s", device->GetQualifiedName().c_str());
    m_devices.emplace_back(std::move(device));
  }

  if (!m_is_populating_devices)
    InvokeDevicesChangedCallbacks();
}

void ControllerInterface::RemoveDevice(std::function<bool(const ciface::Core::Device*)> callback)
{
  {
    std::lock_guard<std::mutex> lk(m_devices_mutex);
    auto it = std::remove_if(m_devices.begin(), m_devices.end(), [&callback](const auto& dev) {
      if (callback(dev.get()))
      {
        NOTICE_LOG(SERIALINTERFACE, "Removed device: %s", dev->GetQualifiedName().c_str());
        return true;
      }
      return false;
    });
    m_devices.erase(it, m_devices.end());
  }

  if (!m_is_populating_devices)
    InvokeDevicesChangedCallbacks();
}

// Update input for all devices if lock can be acquired without waiting.
void ControllerInterface::UpdateInput()
{
  // Don't block the UI or CPU thread (to avoid a short but noticeable frame drop)
  if (m_devices_mutex.try_lock())
  {
    std::lock_guard<std::mutex> lk(m_devices_mutex, std::adopt_lock);
    for (const auto& d : m_devices)
      d->UpdateInput();
  }
}

// Register a callback to be called when a device is added or removed (as from the input backends'
// hotplug thread), or when devices are refreshed
// Returns a handle for later removing the callback.
ControllerInterface::HotplugCallbackHandle
ControllerInterface::RegisterDevicesChangedCallback(std::function<void()> callback)
{
  std::lock_guard<std::mutex> lk(m_callbacks_mutex);
  m_devices_changed_callbacks.emplace_back(std::move(callback));
  return std::prev(m_devices_changed_callbacks.end());
}

// Unregister a device callback.
void ControllerInterface::UnregisterDevicesChangedCallback(const HotplugCallbackHandle& handle)
{
  std::lock_guard<std::mutex> lk(m_callbacks_mutex);
  m_devices_changed_callbacks.erase(handle);
}

// Invoke all callbacks that were registered
void ControllerInterface::InvokeDevicesChangedCallbacks() const
{
  std::lock_guard<std::mutex> lk(m_callbacks_mutex);
  for (const auto& callback : m_devices_changed_callbacks)
    callback();
}
