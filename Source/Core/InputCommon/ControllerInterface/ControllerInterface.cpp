// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <mutex>

#include "InputCommon/ControllerInterface/ControllerInterface.h"

#ifdef CIFACE_USE_XINPUT
#include "InputCommon/ControllerInterface/XInput/XInput.h"
#endif
#ifdef CIFACE_USE_DINPUT
#include "InputCommon/ControllerInterface/DInput/DInput.h"
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

//
// Init
//
// Detect devices and inputs outputs / will make refresh function later
//
void ControllerInterface::Initialize(void* const hwnd)
{
  if (m_is_init)
    return;

  m_hwnd = hwnd;

#ifdef CIFACE_USE_DINPUT
// nothing needed
#endif
#ifdef CIFACE_USE_XINPUT
  ciface::XInput::Init();
#endif
#ifdef CIFACE_USE_XLIB
// nothing needed
#endif
#ifdef CIFACE_USE_OSX
  ciface::OSX::Init(hwnd);
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

  m_is_init = true;
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

#ifdef CIFACE_USE_DINPUT
  ciface::DInput::PopulateDevices(reinterpret_cast<HWND>(m_hwnd));
#endif
#ifdef CIFACE_USE_XINPUT
  ciface::XInput::PopulateDevices();
#endif
#ifdef CIFACE_USE_XLIB
  ciface::XInput2::PopulateDevices(m_hwnd);
#endif
#ifdef CIFACE_USE_OSX
  ciface::OSX::PopulateDevices(m_hwnd);
  ciface::Quartz::PopulateDevices(m_hwnd);
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
}

//
// DeInit
//
// Remove all devices/ call library cleanup functions
//
void ControllerInterface::Shutdown()
{
  if (!m_is_init)
    return;

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

#ifdef CIFACE_USE_XINPUT
  ciface::XInput::DeInit();
#endif
#ifdef CIFACE_USE_DINPUT
// nothing needed
#endif
#ifdef CIFACE_USE_XLIB
// nothing needed
#endif
#ifdef CIFACE_USE_OSX
  ciface::OSX::DeInit();
  ciface::Quartz::DeInit();
#endif
#ifdef CIFACE_USE_SDL
  // TODO: there seems to be some sort of memory leak with SDL, quit isn't freeing everything up
  SDL_Quit();
#endif
#ifdef CIFACE_USE_ANDROID
// nothing needed
#endif
#ifdef CIFACE_USE_EVDEV
  ciface::evdev::Shutdown();
#endif

  m_is_init = false;
}

void ControllerInterface::AddDevice(std::shared_ptr<ciface::Core::Device> device)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  // Try to find an ID for this device
  int id = 0;
  while (true)
  {
    const auto it = std::find_if(m_devices.begin(), m_devices.end(), [&device, &id](const auto& d) {
      return d->GetSource() == device->GetSource() && d->GetName() == device->GetName() &&
             d->GetId() == id;
    });
    if (it == m_devices.end())  // no device with the same name with this ID, so we can use it
      break;
    else
      id++;
  }
  device->SetId(id);
  m_devices.emplace_back(std::move(device));
}

void ControllerInterface::RemoveDevice(std::function<bool(const ciface::Core::Device*)> callback)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  m_devices.erase(std::remove_if(m_devices.begin(), m_devices.end(),
                                 [&callback](const auto& dev) { return callback(dev.get()); }),
                  m_devices.end());
}

//
// UpdateInput
//
// Update input for all devices
//
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

//
// RegisterHotplugCallback
//
// Register a callback to be called from the input backends' hotplug thread
// when there is a new device
//
void ControllerInterface::RegisterHotplugCallback(std::function<void()> callback)
{
  m_hotplug_callbacks.emplace_back(std::move(callback));
}

//
// InvokeHotplugCallbacks
//
// Invoke all callbacks that were registered
//
void ControllerInterface::InvokeHotplugCallbacks() const
{
  for (const auto& callback : m_hotplug_callbacks)
    callback();
}
