// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <fcntl.h>
#include <libudev.h>
#include <map>
#include <memory>
#include <string>
#include <unistd.h>

#include <sys/eventfd.h>

#include "Common/Assert.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/evdev/evdev.h"

namespace ciface::evdev
{
static std::thread s_hotplug_thread;
static Common::Flag s_hotplug_thread_running;
static int s_wakeup_eventfd;

// There is no easy way to get the device name from only a dev node
// during a device removed event, since libevdev can't work on removed devices;
// sysfs is not stable, so this is probably the easiest way to get a name for a node.
static std::map<std::string, std::string> s_devnode_name_map;

static void HotplugThreadFunc()
{
  Common::SetCurrentThreadName("evdev Hotplug Thread");
  NOTICE_LOG(SERIALINTERFACE, "evdev hotplug thread started");

  udev* const udev = udev_new();
  Common::ScopeGuard udev_guard([udev] { udev_unref(udev); });

  ASSERT_MSG(PAD, udev != nullptr, "Couldn't initialize libudev.");

  // Set up monitoring
  udev_monitor* const monitor = udev_monitor_new_from_netlink(udev, "udev");
  Common::ScopeGuard monitor_guard([monitor] { udev_monitor_unref(monitor); });

  udev_monitor_filter_add_match_subsystem_devtype(monitor, "input", nullptr);
  udev_monitor_enable_receiving(monitor);
  const int monitor_fd = udev_monitor_get_fd(monitor);

  while (s_hotplug_thread_running.IsSet())
  {
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(monitor_fd, &fds);
    FD_SET(s_wakeup_eventfd, &fds);

    const int ret =
        select(std::max(monitor_fd, s_wakeup_eventfd) + 1, &fds, nullptr, nullptr, nullptr);
    if (ret < 1 || !FD_ISSET(monitor_fd, &fds))
      continue;

    udev_device* const dev = udev_monitor_receive_device(monitor);
    Common::ScopeGuard dev_guard([dev] { udev_device_unref(dev); });

    const char* const action = udev_device_get_action(dev);
    const char* const devnode = udev_device_get_devnode(dev);
    if (!devnode)
      continue;

    if (strcmp(action, "remove") == 0)
    {
      const auto it = s_devnode_name_map.find(devnode);
      if (it == s_devnode_name_map.end())
      {
        // We don't know the name for this device, so it is probably not an evdev device.
        continue;
      }

      const std::string& name = it->second;
      g_controller_interface.RemoveDevice([&name](const auto& device) {
        return device->GetSource() == "evdev" && device->GetName() == name && !device->IsValid();
      });

      s_devnode_name_map.erase(devnode);
    }
    else if (strcmp(action, "add") == 0)
    {
      const auto device = std::make_shared<evdevDevice>(devnode);
      if (device->IsInteresting())
      {
        s_devnode_name_map.emplace(devnode, device->GetName());
        g_controller_interface.AddDevice(std::move(device));
      }
    }
  }
  NOTICE_LOG(SERIALINTERFACE, "evdev hotplug thread stopped");
}

static void StartHotplugThread()
{
  // Mark the thread as running.
  if (!s_hotplug_thread_running.TestAndSet())
  {
    // It was already running.
    return;
  }

  s_wakeup_eventfd = eventfd(0, 0);
  ASSERT_MSG(PAD, s_wakeup_eventfd != -1, "Couldn't create eventfd.");
  s_hotplug_thread = std::thread(HotplugThreadFunc);
}

static void StopHotplugThread()
{
  // Tell the hotplug thread to stop.
  if (!s_hotplug_thread_running.TestAndClear())
  {
    // It wasn't running, we're done.
    return;
  }

  // Write something to efd so that select() stops blocking.
  const uint64_t value = 1;
  static_cast<void>(write(s_wakeup_eventfd, &value, sizeof(uint64_t)));

  s_hotplug_thread.join();
  close(s_wakeup_eventfd);
}

void Init()
{
  s_devnode_name_map.clear();
  StartHotplugThread();
}

void PopulateDevices()
{
  // We use udev to iterate over all /dev/input/event* devices.
  // Note: the Linux kernel is currently limited to just 32 event devices. If
  // this ever changes, hopefully udev will take care of this.

  udev* const udev = udev_new();
  ASSERT_MSG(PAD, udev != nullptr, "Couldn't initialize libudev.");

  // List all input devices
  udev_enumerate* const enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "input");
  udev_enumerate_scan_devices(enumerate);
  udev_list_entry* const devices = udev_enumerate_get_list_entry(enumerate);

  // Iterate over all input devices
  udev_list_entry* dev_list_entry;
  udev_list_entry_foreach(dev_list_entry, devices)
  {
    const char* path = udev_list_entry_get_name(dev_list_entry);

    udev_device* dev = udev_device_new_from_syspath(udev, path);

    const char* devnode = udev_device_get_devnode(dev);
    if (devnode)
    {
      // Unfortunately udev gives us no way to filter out the non event device interfaces.
      // So we open it and see if it works with evdev ioctls or not.
      const auto input = std::make_shared<evdevDevice>(devnode);

      if (input->IsInteresting())
      {
        s_devnode_name_map.emplace(devnode, input->GetName());
        g_controller_interface.AddDevice(std::move(input));
      }
    }
    udev_device_unref(dev);
  }
  udev_enumerate_unref(enumerate);
  udev_unref(udev);
}

void Shutdown()
{
  StopHotplugThread();
}

evdevDevice::evdevDevice(const std::string& devnode) : m_devfile(devnode)
{
  // The device file will be read on one of the main threads, so we open in non-blocking mode.
  m_fd = open(devnode.c_str(), O_RDWR | O_NONBLOCK);
  if (m_fd == -1)
  {
    return;
  }

  if (libevdev_new_from_fd(m_fd, &m_dev) != 0)
  {
    // This usually fails because the device node isn't an evdev device, such as /dev/input/js0
    close(m_fd);
    m_fd = -1;
    return;
  }

  m_name = StripSpaces(libevdev_get_name(m_dev));

  // Buttons (and keyboard keys)
  int num_buttons = 0;
  for (int key = 0; key < KEY_MAX; key++)
  {
    if (libevdev_has_event_code(m_dev, EV_KEY, key))
      AddInput(new Button(num_buttons++, key, m_dev));
  }

  // Absolute axis (thumbsticks)
  int num_axis = 0;
  for (int axis = 0; axis < 0x100; axis++)
  {
    if (libevdev_has_event_code(m_dev, EV_ABS, axis))
    {
      AddAnalogInputs(new Axis(num_axis, axis, false, m_dev),
                      new Axis(num_axis, axis, true, m_dev));
      ++num_axis;
    }
  }

  // Disable autocenter
  if (libevdev_has_event_code(m_dev, EV_FF, FF_AUTOCENTER))
  {
    input_event ie = {};
    ie.type = EV_FF;
    ie.code = FF_AUTOCENTER;
    ie.value = 0;

    static_cast<void>(write(m_fd, &ie, sizeof(ie)));
  }

  // Constant FF effect
  if (libevdev_has_event_code(m_dev, EV_FF, FF_CONSTANT))
  {
    AddOutput(new ConstantEffect(m_fd));
  }

  // Periodic FF effects
  if (libevdev_has_event_code(m_dev, EV_FF, FF_PERIODIC))
  {
    for (auto wave : {FF_SINE, FF_SQUARE, FF_TRIANGLE, FF_SAW_UP, FF_SAW_DOWN})
    {
      if (libevdev_has_event_code(m_dev, EV_FF, wave))
        AddOutput(new PeriodicEffect(m_fd, wave));
    }
  }

  // Rumble (i.e. Left/Right) (i.e. Strong/Weak) effect
  if (libevdev_has_event_code(m_dev, EV_FF, FF_RUMBLE))
  {
    AddOutput(new RumbleEffect(m_fd, RumbleEffect::Motor::Strong));
    AddOutput(new RumbleEffect(m_fd, RumbleEffect::Motor::Weak));
  }

  // TODO: Add leds as output devices

  // Was there some reasoning behind these numbers?
  m_interesting = num_axis >= 2 || num_buttons >= 8;
}

evdevDevice::~evdevDevice()
{
  if (m_fd != -1)
  {
    libevdev_free(m_dev);
    close(m_fd);
  }
}

void evdevDevice::UpdateInput()
{
  // Run through all evdev events
  // libevdev will keep track of the actual controller state internally which can be queried
  // later with libevdev_fetch_event_value()
  int rc = LIBEVDEV_READ_STATUS_SUCCESS;
  while (rc >= 0)
  {
    input_event ev;
    if (LIBEVDEV_READ_STATUS_SYNC == rc)
      rc = libevdev_next_event(m_dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
    else
      rc = libevdev_next_event(m_dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
  }
}

bool evdevDevice::IsValid() const
{
  int current_fd = libevdev_get_fd(m_dev);
  if (current_fd == -1)
    return false;

  libevdev* device;
  if (libevdev_new_from_fd(current_fd, &device) != 0)
  {
    close(current_fd);
    return false;
  }
  libevdev_free(device);
  return true;
}

std::string evdevDevice::Button::GetName() const
{
  // Buttons below 0x100 are mostly keyboard keys, and the names make sense
  if (m_code < 0x100)
  {
    const char* name = libevdev_event_code_get_name(EV_KEY, m_code);
    if (name)
      return StripSpaces(name);
  }
  // But controllers use codes above 0x100, and the standard label often doesn't match.
  // We are better off with Button 0 and so on.
  return "Button " + std::to_string(m_index);
}

ControlState evdevDevice::Button::GetState() const
{
  int value = 0;
  libevdev_fetch_event_value(m_dev, EV_KEY, m_code, &value);
  return value;
}

evdevDevice::Axis::Axis(u8 index, u16 code, bool upper, libevdev* dev)
    : m_code(code), m_index(index), m_dev(dev)
{
  const int min = libevdev_get_abs_minimum(m_dev, m_code);
  const int max = libevdev_get_abs_maximum(m_dev, m_code);

  m_base = (max + min) / 2;
  m_range = (upper ? max : min) - m_base;
}

std::string evdevDevice::Axis::GetName() const
{
  return "Axis " + std::to_string(m_index) + (m_range < 0 ? '-' : '+');
}

ControlState evdevDevice::Axis::GetState() const
{
  int value = 0;
  libevdev_fetch_event_value(m_dev, EV_ABS, m_code, &value);

  return ControlState(value - m_base) / m_range;
}

evdevDevice::Effect::Effect(int fd) : m_fd(fd)
{
  m_effect.id = -1;
  // Left (for wheels):
  m_effect.direction = 0x4000;
  m_effect.replay.length = RUMBLE_LENGTH_MS;

  // FYI: type is set within UpdateParameters.
  m_effect.type = DISABLED_EFFECT_TYPE;
}

std::string evdevDevice::ConstantEffect::GetName() const
{
  return "Constant";
}

std::string evdevDevice::PeriodicEffect::GetName() const
{
  switch (m_effect.u.periodic.waveform)
  {
  case FF_SQUARE:
    return "Square";
  case FF_TRIANGLE:
    return "Triangle";
  case FF_SINE:
    return "Sine";
  case FF_SAW_UP:
    return "Sawtooth Up";
  case FF_SAW_DOWN:
    return "Sawtooth Down";
  default:
    return "Unknown";
  }
}

std::string evdevDevice::RumbleEffect::GetName() const
{
  return (Motor::Strong == m_motor) ? "Strong" : "Weak";
}

void evdevDevice::Effect::SetState(ControlState state)
{
  if (UpdateParameters(state))
  {
    // Update effect if parameters changed.
    UpdateEffect();
  }
}

void evdevDevice::Effect::UpdateEffect()
{
  // libevdev doesn't have nice helpers for forcefeedback
  // we will use the file descriptors directly.

  // Note: m_effect.type is set within UpdateParameters
  // to determine if effect should be playing or not.
  if (m_effect.type != DISABLED_EFFECT_TYPE)
  {
    if (-1 == m_effect.id)
    {
      // If effect was not uploaded (previously stopped)
      // we upload it and start playback

      ioctl(m_fd, EVIOCSFF, &m_effect);

      input_event play = {};
      play.type = EV_FF;
      play.code = m_effect.id;
      play.value = 1;

      static_cast<void>(write(m_fd, &play, sizeof(play)));
    }
    else
    {
      // Effect is already playing. Just update parameters.
      ioctl(m_fd, EVIOCSFF, &m_effect);
    }
  }
  else
  {
    // Stop and remove effect.
    ioctl(m_fd, EVIOCRMFF, m_effect.id);
    m_effect.id = -1;
  }
}

evdevDevice::ConstantEffect::ConstantEffect(int fd) : Effect(fd)
{
  m_effect.u.constant = {};
}

evdevDevice::PeriodicEffect::PeriodicEffect(int fd, u16 waveform) : Effect(fd)
{
  m_effect.u.periodic = {};
  m_effect.u.periodic.waveform = waveform;
  m_effect.u.periodic.period = RUMBLE_PERIOD_MS;
  m_effect.u.periodic.offset = 0;
  m_effect.u.periodic.phase = 0;
}

evdevDevice::RumbleEffect::RumbleEffect(int fd, Motor motor) : Effect(fd), m_motor(motor)
{
  m_effect.u.rumble = {};
}

bool evdevDevice::ConstantEffect::UpdateParameters(ControlState state)
{
  s16& value = m_effect.u.constant.level;
  const s16 old_value = value;

  constexpr s16 MAX_VALUE = 0x7fff;
  value = s16(state * MAX_VALUE);

  m_effect.type = value ? FF_CONSTANT : DISABLED_EFFECT_TYPE;
  return value != old_value;
}

bool evdevDevice::PeriodicEffect::UpdateParameters(ControlState state)
{
  s16& value = m_effect.u.periodic.magnitude;
  const s16 old_value = value;

  constexpr s16 MAX_VALUE = 0x7fff;
  value = s16(state * MAX_VALUE);

  m_effect.type = value ? FF_PERIODIC : DISABLED_EFFECT_TYPE;
  return value != old_value;
}

bool evdevDevice::RumbleEffect::UpdateParameters(ControlState state)
{
  u16& value = (Motor::Strong == m_motor) ? m_effect.u.rumble.strong_magnitude :
                                            m_effect.u.rumble.weak_magnitude;
  const u16 old_value = value;

  constexpr u16 MAX_VALUE = 0xffff;
  value = u16(state * MAX_VALUE);

  m_effect.type = value ? FF_RUMBLE : DISABLED_EFFECT_TYPE;
  return value != old_value;
}

evdevDevice::Effect::~Effect()
{
  m_effect.type = DISABLED_EFFECT_TYPE;
  UpdateEffect();
}
}  // namespace ciface::evdev
