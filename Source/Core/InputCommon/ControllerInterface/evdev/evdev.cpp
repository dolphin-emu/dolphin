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
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/evdev/evdev.h"

namespace ciface
{
namespace evdev
{
static std::thread s_hotplug_thread;
static Common::Flag s_hotplug_thread_running;
static int s_wakeup_eventfd;

// There is no easy way to get the device name from only a dev node
// during a device removed event, since libevdev can't work on removed devices;
// sysfs is not stable, so this is probably the easiest way to get a name for a node.
static std::map<std::string, std::string> s_devnode_name_map;

static std::string GetName(const std::string& devnode)
{
  int fd = open(devnode.c_str(), O_RDWR | O_NONBLOCK);
  libevdev* dev = nullptr;
  int ret = libevdev_new_from_fd(fd, &dev);
  if (ret != 0)
  {
    close(fd);
    return std::string();
  }
  std::string res = StripSpaces(libevdev_get_name(dev));
  libevdev_free(dev);
  close(fd);
  return res;
}

static void HotplugThreadFunc()
{
  Common::SetCurrentThreadName("evdev Hotplug Thread");
  NOTICE_LOG(SERIALINTERFACE, "evdev hotplug thread started");

  udev* udev = udev_new();
  _assert_msg_(PAD, udev != nullptr, "Couldn't initialize libudev.");

  // Set up monitoring
  udev_monitor* monitor = udev_monitor_new_from_netlink(udev, "udev");
  udev_monitor_filter_add_match_subsystem_devtype(monitor, "input", nullptr);
  udev_monitor_enable_receiving(monitor);
  const int monitor_fd = udev_monitor_get_fd(monitor);

  while (s_hotplug_thread_running.IsSet())
  {
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(monitor_fd, &fds);
    FD_SET(s_wakeup_eventfd, &fds);

    int ret = select(monitor_fd + 1, &fds, nullptr, nullptr, nullptr);
    if (ret < 1 || !FD_ISSET(monitor_fd, &fds))
      continue;

    udev_device* dev = udev_monitor_receive_device(monitor);

    const char* action = udev_device_get_action(dev);
    const char* devnode = udev_device_get_devnode(dev);
    if (!devnode)
      continue;

    if (strcmp(action, "remove") == 0)
    {
      const auto it = s_devnode_name_map.find(devnode);
      if (it == s_devnode_name_map.end())
        continue;  // we don't know the name for this device, so it is probably not an evdev device
      const std::string& name = it->second;
      g_controller_interface.RemoveDevice([&name](const auto& device) {
        return device->GetSource() == "evdev" && device->GetName() == name && !device->IsValid();
      });
      NOTICE_LOG(SERIALINTERFACE, "Removed device: %s", name.c_str());
      s_devnode_name_map.erase(devnode);
      g_controller_interface.InvokeHotplugCallbacks();
    }
    // Only react to "device added" events for evdev devices that we can access.
    else if (strcmp(action, "add") == 0 && access(devnode, W_OK) == 0)
    {
      const std::string name = GetName(devnode);
      if (name.empty())
        continue;  // probably not an evdev device
      auto device = std::make_shared<evdevDevice>(devnode);
      if (device->IsInteresting())
      {
        g_controller_interface.AddDevice(std::move(device));
        s_devnode_name_map.insert(std::pair<std::string, std::string>(devnode, name));
        NOTICE_LOG(SERIALINTERFACE, "Added new device: %s", name.c_str());
        g_controller_interface.InvokeHotplugCallbacks();
      }
    }
    udev_device_unref(dev);
  }
  NOTICE_LOG(SERIALINTERFACE, "evdev hotplug thread stopped");
}

static void StartHotplugThread()
{
  // Mark the thread as running.
  if (!s_hotplug_thread_running.TestAndSet())
    // It was already running.
    return;

  s_wakeup_eventfd = eventfd(0, 0);
  _assert_msg_(PAD, s_wakeup_eventfd != -1, "Couldn't create eventfd.");
  s_hotplug_thread = std::thread(HotplugThreadFunc);
}

static void StopHotplugThread()
{
  // Tell the hotplug thread to stop.
  if (!s_hotplug_thread_running.TestAndClear())
    // It wasn't running, we're done.
    return;
  // Write something to efd so that select() stops blocking.
  uint64_t value = 1;
  if (write(s_wakeup_eventfd, &value, sizeof(uint64_t)) < 0)
  {
  }
  s_hotplug_thread.join();
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

  udev* udev = udev_new();
  _assert_msg_(PAD, udev != nullptr, "Couldn't initialize libudev.");

  // List all input devices
  udev_enumerate* enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "input");
  udev_enumerate_scan_devices(enumerate);
  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);

  // Iterate over all input devices
  udev_list_entry* dev_list_entry;
  udev_list_entry_foreach(dev_list_entry, devices)
  {
    const char* path = udev_list_entry_get_name(dev_list_entry);

    udev_device* dev = udev_device_new_from_syspath(udev, path);

    const char* devnode = udev_device_get_devnode(dev);
    // We only care about devices which we have read/write access to.
    if (devnode && access(devnode, W_OK) == 0)
    {
      // Unfortunately udev gives us no way to filter out the non event device interfaces.
      // So we open it and see if it works with evdev ioctls or not.
      auto input = std::make_shared<evdevDevice>(devnode);

      if (input->IsInteresting())
      {
        g_controller_interface.AddDevice(std::move(input));
        std::string name = GetName(devnode);
        s_devnode_name_map.insert(std::pair<std::string, std::string>(devnode, name));
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
  int ret = libevdev_new_from_fd(m_fd, &m_dev);

  if (ret != 0)
  {
    // This useally fails because the device node isn't an evdev device, such as /dev/input/js0
    m_initialized = false;
    close(m_fd);
    return;
  }

  m_name = StripSpaces(libevdev_get_name(m_dev));

  // Controller buttons (and keyboard keys)
  int num_buttons = 0;
  for (int key = 0; key < KEY_MAX; key++)
    if (libevdev_has_event_code(m_dev, EV_KEY, key))
      AddInput(new Button(num_buttons++, key, m_dev));

  // Absolute axis (thumbsticks)
  int num_axis = 0;
  for (int axis = 0; axis < 0x100; axis++)
    if (libevdev_has_event_code(m_dev, EV_ABS, axis))
    {
      AddAnalogInputs(new Axis(num_axis, axis, false, m_dev),
                      new Axis(num_axis, axis, true, m_dev));
      num_axis++;
    }

  // Force feedback
  if (libevdev_has_event_code(m_dev, EV_FF, FF_PERIODIC))
  {
    for (auto type : {FF_SINE, FF_SQUARE, FF_TRIANGLE, FF_SAW_UP, FF_SAW_DOWN})
      if (libevdev_has_event_code(m_dev, EV_FF, type))
        AddOutput(new ForceFeedback(type, m_dev));
  }
  if (libevdev_has_event_code(m_dev, EV_FF, FF_RUMBLE))
  {
    AddOutput(new ForceFeedback(FF_RUMBLE, m_dev));
  }

  // TODO: Add leds as output devices

  m_initialized = true;
  m_interesting = num_axis >= 2 || num_buttons >= 8;
}

evdevDevice::~evdevDevice()
{
  if (m_initialized)
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
  input_event ev;
  int rc = LIBEVDEV_READ_STATUS_SUCCESS;
  do
  {
    if (rc == LIBEVDEV_READ_STATUS_SYNC)
      rc = libevdev_next_event(m_dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
    else
      rc = libevdev_next_event(m_dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
  } while (rc >= 0);
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
    : m_code(code), m_index(index), m_upper(upper), m_dev(dev)
{
  m_min = libevdev_get_abs_minimum(m_dev, m_code);
  m_range = libevdev_get_abs_maximum(m_dev, m_code) - m_min;
}

std::string evdevDevice::Axis::GetName() const
{
  return "Axis " + std::to_string(m_index) + (m_upper ? "+" : "-");
}

ControlState evdevDevice::Axis::GetState() const
{
  int value = 0;
  libevdev_fetch_event_value(m_dev, EV_ABS, m_code, &value);

  // Value from 0.0 to 1.0
  ControlState fvalue = MathUtil::Clamp(double(value - m_min) / double(m_range), 0.0, 1.0);

  // Split into two axis, each covering half the range from 0.0 to 1.0
  if (m_upper)
    return std::max(0.0, fvalue - 0.5) * 2.0;
  else
    return (0.5 - std::min(0.5, fvalue)) * 2.0;
}

std::string evdevDevice::ForceFeedback::GetName() const
{
  // We have some default names.
  switch (m_type)
  {
  case FF_SINE:
    return "Sine";
  case FF_TRIANGLE:
    return "Triangle";
  case FF_SQUARE:
    return "Square";
  case FF_RUMBLE:
    return "LeftRight";
  default:
  {
    const char* name = libevdev_event_code_get_name(EV_FF, m_type);
    if (name)
      return StripSpaces(name);
    return "Unknown";
  }
  }
}

void evdevDevice::ForceFeedback::SetState(ControlState state)
{
  // libevdev doesn't have nice helpers for forcefeedback
  // we will use the file descriptors directly.

  if (m_id != -1)  // delete the previous effect (which also stops it)
  {
    ioctl(m_fd, EVIOCRMFF, m_id);
    m_id = -1;
  }

  if (state > 0)  // Upload and start an effect.
  {
    ff_effect effect;

    effect.id = -1;
    effect.direction = 0;        // down
    effect.replay.length = 500;  // 500ms
    effect.replay.delay = 0;
    effect.trigger.button = 0;  // don't trigger on button press
    effect.trigger.interval = 0;

    // This is the the interface that XInput uses, with 2 motors of differing sizes/frequencies that
    // are controlled seperatally
    if (m_type == FF_RUMBLE)
    {
      effect.type = FF_RUMBLE;
      // max ranges tuned to 'feel' similar in magnitude to triangle/sine on xbox360 controller
      effect.u.rumble.strong_magnitude = u16(state * 0x4000);
      effect.u.rumble.weak_magnitude = u16(state * 0xFFFF);
    }
    else  // FF_PERIODIC, a more generic interface.
    {
      effect.type = FF_PERIODIC;
      effect.u.periodic.waveform = m_type;
      effect.u.periodic.phase = 0x7fff;  // 180 degrees
      effect.u.periodic.offset = 0;
      effect.u.periodic.period = 10;
      effect.u.periodic.magnitude = s16(state * 0x7FFF);
      effect.u.periodic.envelope.attack_length = 0;  // no attack
      effect.u.periodic.envelope.attack_level = 0;
      effect.u.periodic.envelope.fade_length = 0;
      effect.u.periodic.envelope.fade_level = 0;
    }

    ioctl(m_fd, EVIOCSFF, &effect);
    m_id = effect.id;

    input_event play;
    play.type = EV_FF;
    play.code = m_id;
    play.value = 1;

    if (write(m_fd, &play, sizeof(play)) < 0)
    {
    }
  }
}

evdevDevice::ForceFeedback::~ForceFeedback()
{
  // delete the uploaded effect, so we don't leak it.
  if (m_id != -1)
  {
    ioctl(m_fd, EVIOCRMFF, m_id);
  }
}
}
}
