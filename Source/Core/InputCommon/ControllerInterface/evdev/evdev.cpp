// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/evdev/evdev.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>

#include <fcntl.h>
#include <libudev.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "Common/Assert.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::evdev
{
class InputBackend final : public ciface::InputBackend
{
public:
  InputBackend(ControllerInterface* controller_interface);
  ~InputBackend();
  void PopulateDevices() override;

  void RemoveDevnodeObject(const std::string&);

private:
  std::shared_ptr<evdevDevice>
  FindDeviceWithUniqueIDAndPhysicalLocation(const char* unique_id, const char* physical_location);

  void AddDeviceNode(const char* devnode);

  void StartHotplugThread();
  void StopHotplugThread();
  void HotplugThreadFunc();

  std::thread m_hotplug_thread;
  Common::Flag m_hotplug_thread_running;
  int m_wakeup_eventfd;

  // There is no easy way to get the device name from only a dev node
  // during a device removed event, since libevdev can't work on removed devices;
  // sysfs is not stable, so this is probably the easiest way to get a name for a node.
  // This can, and will be modified by different thread, possibly concurrently,
  // as devices can be destroyed by any thread at any time. As of now it's protected
  // by ControllerInterface::m_devices_population_mutex.
  std::map<std::string, std::weak_ptr<evdevDevice>> m_devnode_objects;
};

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface)
{
  return std::make_unique<InputBackend>(controller_interface);
}

class Input : public Core::Device::Input
{
public:
  Input(u16 code, libevdev* dev) : m_code(code), m_dev(dev) {}

protected:
  const u16 m_code;
  libevdev* const m_dev;
};

class Button : public Input
{
public:
  Button(u8 index, u16 code, libevdev* dev) : Input(code, dev), m_index(index) {}

  ControlState GetState() const final override
  {
    int value = 0;
    libevdev_fetch_event_value(m_dev, EV_KEY, m_code, &value);
    return value;
  }

protected:
  std::optional<std::string> GetEventCodeName() const
  {
    if (const char* code_name = libevdev_event_code_get_name(EV_KEY, m_code))
    {
      const auto name = StripWhitespace(code_name);

      for (auto remove_prefix : {"BTN_", "KEY_"})
      {
        if (name.find(remove_prefix) == 0)
          return std::string(name.substr(std::strlen(remove_prefix)));
      }

      return std::string(name);
    }
    else
    {
      return std::nullopt;
    }
  }

  std::string GetIndexedName() const { return "Button " + std::to_string(m_index); }

  const u8 m_index;
};

class NumberedButton final : public Button
{
public:
  using Button::Button;

  std::string GetName() const override { return GetIndexedName(); }
};

class NamedButton final : public Button
{
public:
  using Button::Button;

  bool IsMatchingName(std::string_view name) const final override
  {
    // Match either the "START" naming provided by evdev or the "Button 0"-like naming.
    return name == GetEventCodeName() || name == GetIndexedName();
  }

  std::string GetName() const override { return GetEventCodeName().value_or(GetIndexedName()); }
};

class NamedButtonWithNoBackwardsCompat final : public Button
{
public:
  using Button::Button;

  std::string GetName() const override { return GetEventCodeName().value_or(GetIndexedName()); }
};

class AnalogInput : public Input
{
public:
  using Input::Input;

  ControlState GetState() const final override
  {
    int value = 0;
    libevdev_fetch_event_value(m_dev, EV_ABS, m_code, &value);

    return (value - m_base) / m_range;
  }

protected:
  ControlState m_range;
  int m_base;
};

class Axis : public AnalogInput
{
public:
  Axis(u8 index, u16 code, bool upper, libevdev* dev) : AnalogInput(code, dev), m_index(index)
  {
    const int min = libevdev_get_abs_minimum(m_dev, m_code);
    const int max = libevdev_get_abs_maximum(m_dev, m_code);

    m_base = (max + min) / 2;
    m_range = (upper ? max : min) - m_base;
  }

  std::string GetName() const override { return GetIndexedName(); }

protected:
  std::string GetIndexedName() const
  {
    return "Axis " + std::to_string(m_index) + (m_range < 0 ? '-' : '+');
  }

private:
  const u8 m_index;
};

class MotionDataInput final : public AnalogInput
{
public:
  MotionDataInput(u16 code, ControlState resolution_scale, libevdev* dev) : AnalogInput(code, dev)
  {
    auto* const info = libevdev_get_abs_info(m_dev, m_code);

    // The average of the minimum and maximum value. (neutral value)
    m_base = (info->maximum + info->minimum) / 2;

    m_range = info->resolution / resolution_scale;
  }

  std::string GetName() const override
  {
    // Unfortunately there doesn't seem to be a "standard" orientation
    // so we can't use "Accel Up"-like names.
    constexpr std::array<const char*, 6> motion_data_names = {{
        "Accel X",
        "Accel Y",
        "Accel Z",
        "Gyro X",
        "Gyro Y",
        "Gyro Z",
    }};

    // Our name array relies on sane axis codes from 0 to 5.
    static_assert(ABS_X == 0, "evdev axis value sanity check");
    static_assert(ABS_RX == 3, "evdev axis value sanity check");

    return std::string(motion_data_names[m_code]) + (m_range < 0 ? '-' : '+');
  }

  bool IsDetectable() const override { return false; }
};

class CursorInput final : public Axis
{
public:
  using Axis::Axis;

  std::string GetName() const final override
  {
    // "Cursor X-" naming.
    return std::string("Cursor ") + char('X' + m_code) + (m_range < 0 ? '-' : '+');
  }

  bool IsDetectable() const override { return false; }
};

std::shared_ptr<evdevDevice>
InputBackend::FindDeviceWithUniqueIDAndPhysicalLocation(const char* unique_id,
                                                        const char* physical_location)
{
  if (!unique_id || !physical_location)
    return nullptr;

  for (auto& [node, dev] : m_devnode_objects)
  {
    if (const auto device = dev.lock())
    {
      const auto* dev_uniq = device->GetUniqueID();
      const auto* dev_phys = device->GetPhysicalLocation();

      if (dev_uniq && dev_phys && std::strcmp(dev_uniq, unique_id) == 0 &&
          std::strcmp(dev_phys, physical_location) == 0)
      {
        return device;
      }
    }
  }

  return nullptr;
}

void InputBackend::AddDeviceNode(const char* devnode)
{
  // Unfortunately udev gives us no way to filter out the non event device interfaces.
  // So we open it and see if it works with evdev ioctls or not.

  // The device file will be read on one of the main threads, so we open in non-blocking mode.
  const int fd = open(devnode, O_RDWR | O_NONBLOCK);
  if (fd == -1)
  {
    return;
  }

  libevdev* dev = nullptr;
  if (libevdev_new_from_fd(fd, &dev) != 0)
  {
    // This usually fails because the device node isn't an evdev device, such as /dev/input/js0
    close(fd);
    return;
  }

  const auto uniq = libevdev_get_uniq(dev);
  const auto phys = libevdev_get_phys(dev);
  auto evdev_device = FindDeviceWithUniqueIDAndPhysicalLocation(uniq, phys);
  if (evdev_device)
  {
    NOTICE_LOG_FMT(CONTROLLERINTERFACE,
                   "evdev combining devices with unique id: {}, physical location: {}", uniq, phys);

    evdev_device->AddNode(devnode, fd, dev);

    // Remove and re-add device as naming and inputs may have changed.
    // This will also give it the correct index and invoke device change callbacks.
    // Make sure to force the device removal immediately (as they are shared ptrs and
    // they could be kept alive, preventing us from re-creating the device)
    GetControllerInterface().RemoveDevice(
        [&evdev_device](const auto* device) {
          return static_cast<const evdevDevice*>(device) == evdev_device.get();
        },
        true);

    GetControllerInterface().AddDevice(evdev_device);
  }
  else
  {
    evdev_device = std::make_shared<evdevDevice>(this);

    const bool was_interesting = evdev_device->AddNode(devnode, fd, dev);

    if (was_interesting)
      GetControllerInterface().AddDevice(evdev_device);
  }

  // If the devices failed to be added to ControllerInterface, it will be added here but then
  // immediately removed in its destructor due to the shared ptr not having any references left
  m_devnode_objects.emplace(devnode, std::move(evdev_device));
}

void InputBackend::HotplugThreadFunc()
{
  Common::SetCurrentThreadName("evdev Hotplug Thread");
  NOTICE_LOG_FMT(CONTROLLERINTERFACE, "evdev hotplug thread started");

  udev* const udev = udev_new();
  Common::ScopeGuard udev_guard([udev] { udev_unref(udev); });

  ASSERT_MSG(CONTROLLERINTERFACE, udev != nullptr, "Couldn't initialize libudev.");

  // Set up monitoring
  udev_monitor* const monitor = udev_monitor_new_from_netlink(udev, "udev");
  Common::ScopeGuard monitor_guard([monitor] { udev_monitor_unref(monitor); });

  udev_monitor_filter_add_match_subsystem_devtype(monitor, "input", nullptr);
  udev_monitor_enable_receiving(monitor);
  const int monitor_fd = udev_monitor_get_fd(monitor);

  while (m_hotplug_thread_running.IsSet())
  {
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(monitor_fd, &fds);
    FD_SET(m_wakeup_eventfd, &fds);

    const int ret =
        select(std::max(monitor_fd, m_wakeup_eventfd) + 1, &fds, nullptr, nullptr, nullptr);
    if (ret < 1 || !FD_ISSET(monitor_fd, &fds))
      continue;

    udev_device* const dev = udev_monitor_receive_device(monitor);
    Common::ScopeGuard dev_guard([dev] { udev_device_unref(dev); });

    const char* const action = udev_device_get_action(dev);
    const char* const devnode = udev_device_get_devnode(dev);
    if (!devnode)
      continue;

    // Use GetControllerInterface().PlatformPopulateDevices() to protect access around
    // m_devnode_objects. Note that even if we get these events at the same time as a
    // a PopulateDevices() request (e.g. on start up, we might get all the add events
    // for connected devices), this won't ever cause duplicate devices as AddDeviceNode()
    // automatically removes the old one if it already existed
    if (strcmp(action, "remove") == 0)
    {
      GetControllerInterface().PlatformPopulateDevices([&devnode, this] {
        std::shared_ptr<evdevDevice> ptr;

        const auto it = m_devnode_objects.find(devnode);
        if (it != m_devnode_objects.end())
          ptr = it->second.lock();

        // If we don't recognize this device, ptr will be null and no device will be removed.

        GetControllerInterface().RemoveDevice([&ptr](const auto* device) {
          return static_cast<const evdevDevice*>(device) == ptr.get();
        });
      });
    }
    else if (strcmp(action, "add") == 0)
    {
      GetControllerInterface().PlatformPopulateDevices(
          [&devnode, this] { AddDeviceNode(devnode); });
    }
  }
  NOTICE_LOG_FMT(CONTROLLERINTERFACE, "evdev hotplug thread stopped");
}

void InputBackend::StartHotplugThread()
{
  // Mark the thread as running.
  if (!m_hotplug_thread_running.TestAndSet())
  {
    // It was already running.
    return;
  }

  m_wakeup_eventfd = eventfd(0, 0);
  ASSERT_MSG(CONTROLLERINTERFACE, m_wakeup_eventfd != -1, "Couldn't create eventfd.");
  m_hotplug_thread = std::thread(&InputBackend::HotplugThreadFunc, this);
}

void InputBackend::StopHotplugThread()
{
  // Tell the hotplug thread to stop.
  if (!m_hotplug_thread_running.TestAndClear())
  {
    // It wasn't running, we're done.
    return;
  }

  // Write something to efd so that select() stops blocking.
  const uint64_t value = 1;
  static_cast<void>(!write(m_wakeup_eventfd, &value, sizeof(uint64_t)));

  m_hotplug_thread.join();
  close(m_wakeup_eventfd);
}

InputBackend::InputBackend(ControllerInterface* controller_interface)
    : ciface::InputBackend(controller_interface)
{
  StartHotplugThread();
}

// Only call this when ControllerInterface::m_devices_population_mutex is locked
void InputBackend::PopulateDevices()
{
  // Don't run if we are not initialized
  if (!m_hotplug_thread_running.IsSet())
  {
    return;
  }

  // We use udev to iterate over all /dev/input/event* devices.
  // Note: the Linux kernel is currently limited to just 32 event devices. If
  // this ever changes, hopefully udev will take care of this.

  udev* const udev = udev_new();
  ASSERT_MSG(CONTROLLERINTERFACE, udev != nullptr, "Couldn't initialize libudev.");

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

    if (const char* devnode = udev_device_get_devnode(dev))
      AddDeviceNode(devnode);

    udev_device_unref(dev);
  }
  udev_enumerate_unref(enumerate);
  udev_unref(udev);
}

InputBackend::~InputBackend()
{
  StopHotplugThread();
}

bool evdevDevice::AddNode(std::string devnode, int fd, libevdev* dev)
{
  m_nodes.emplace_back(Node{std::move(devnode), fd, dev});

  // Take on the alphabetically first name.
  const auto potential_new_name = StripWhitespace(libevdev_get_name(dev));
  if (m_name.empty() || potential_new_name < m_name)
    m_name = potential_new_name;

  const bool is_motion_device = libevdev_has_property(dev, INPUT_PROP_ACCELEROMETER);
  const bool is_pointing_device = libevdev_has_property(dev, INPUT_PROP_BUTTONPAD);

  // If a device has BTN_JOYSTICK it probably uses event codes counting up from 0x120
  // which have very useless and wrong names.
  const bool has_btn_joystick = libevdev_has_event_code(dev, EV_KEY, BTN_JOYSTICK);
  const bool has_sensible_button_names = !has_btn_joystick;

  // Buttons (and keyboard keys)
  int num_buttons = 0;
  for (int key = 0; key != KEY_CNT; ++key)
  {
    if (libevdev_has_event_code(dev, EV_KEY, key))
    {
      if (is_pointing_device || is_motion_device)
      {
        // This node will probably be combined with another with regular buttons.
        // We don't want to match "Button 0" names here as it will name clash.
        AddInput(new NamedButtonWithNoBackwardsCompat(num_buttons, key, dev));
      }
      else if (has_sensible_button_names)
      {
        AddInput(new NamedButton(num_buttons, key, dev));
      }
      else
      {
        AddInput(new NumberedButton(num_buttons, key, dev));
      }

      ++num_buttons;
    }
  }

  int num_axis = 0;

  if (is_motion_device)
  {
    // If INPUT_PROP_ACCELEROMETER is set then X,Y,Z,RX,RY,RZ contain motion data.

    auto add_motion_inputs = [&num_axis, dev, this](int first_code, double scale) {
      for (int i = 0; i != 3; ++i)
      {
        const int code = first_code + i;
        if (libevdev_has_event_code(dev, EV_ABS, code))
        {
          AddInput(new MotionDataInput(code, scale * -1, dev));
          AddInput(new MotionDataInput(code, scale, dev));

          ++num_axis;
        }
      }
    };

    // evdev resolution is specified in "g"s and deg/s.
    // Convert these to m/s/s and rad/s.
    constexpr ControlState accel_scale = MathUtil::GRAVITY_ACCELERATION;
    constexpr ControlState gyro_scale = MathUtil::TAU / 360;

    add_motion_inputs(ABS_X, accel_scale);
    add_motion_inputs(ABS_RX, gyro_scale);

    return true;
  }

  if (is_pointing_device)
  {
    auto add_cursor_input = [&num_axis, dev, this](int code) {
      if (libevdev_has_event_code(dev, EV_ABS, code))
      {
        AddInput(new CursorInput(num_axis, code, false, dev));
        AddInput(new CursorInput(num_axis, code, true, dev));

        ++num_axis;
      }
    };

    add_cursor_input(ABS_X);
    add_cursor_input(ABS_Y);

    return true;
  }

  // Axes beyond ABS_MISC have strange behavior (for multi-touch) which we do not handle.
  const int abs_axis_code_count = ABS_MISC;

  // Absolute axis (thumbsticks)
  for (int axis = 0; axis != abs_axis_code_count; ++axis)
  {
    if (libevdev_has_event_code(dev, EV_ABS, axis))
    {
      AddAnalogInputs(new Axis(num_axis, axis, false, dev), new Axis(num_axis, axis, true, dev));
      ++num_axis;
    }
  }

  // Disable autocenter
  if (libevdev_has_event_code(dev, EV_FF, FF_AUTOCENTER))
  {
    input_event ie = {};
    ie.type = EV_FF;
    ie.code = FF_AUTOCENTER;
    ie.value = 0;

    static_cast<void>(!write(fd, &ie, sizeof(ie)));
  }

  // Constant FF effect
  if (libevdev_has_event_code(dev, EV_FF, FF_CONSTANT))
  {
    AddOutput(new ConstantEffect(fd));
  }

  // Periodic FF effects
  if (libevdev_has_event_code(dev, EV_FF, FF_PERIODIC))
  {
    for (auto wave : {FF_SINE, FF_SQUARE, FF_TRIANGLE, FF_SAW_UP, FF_SAW_DOWN})
    {
      if (libevdev_has_event_code(dev, EV_FF, wave))
        AddOutput(new PeriodicEffect(fd, wave));
    }
  }

  // Rumble (i.e. Left/Right) (i.e. Strong/Weak) effect
  if (libevdev_has_event_code(dev, EV_FF, FF_RUMBLE))
  {
    AddOutput(new RumbleEffect(fd, RumbleEffect::Motor::Strong));
    AddOutput(new RumbleEffect(fd, RumbleEffect::Motor::Weak));
  }

  // TODO: Add leds as output devices

  // Filter out interesting devices (see description below)
  return num_axis >= 2 || num_buttons >= 8;

  // On modern linux systems, there are a lot of event devices that aren't controllers.
  // For example, the PC Speaker is an event device. Webcams sometimes show up as
  // event devices. The power button is an event device.
  //
  // We don't want these showing up in the list of controllers, so we use this
  // heuristic to filter out anything that doesn't smell like a controller:
  //
  // More than two analog axis:
  //    Most controllers have at least one stick. This rule will catch all such
  //    controllers, while ignoring anything with a single axis (like the mouse
  //    scroll-wheel)
  //
  // --- OR ---
  //
  // More than 8 buttons:
  //     The user might be using a digital only pad such as a NES controller.
  //     This rule caches such controllers, while eliminating any device with
  //     only a few buttons, like the power button. Sometimes laptops have devices
  //     with 5 or 6 special buttons, which is why the threshold is set to 8 to
  //     match a NES controller.
  //
  // This heuristic is quite loose. The user may still see weird devices showing up
  // as controllers, but it hopefully shouldn't filter out anything they actually
  // want to use.
}

const char* evdevDevice::GetUniqueID() const
{
  if (m_nodes.empty())
    return nullptr;

  const auto uniq = libevdev_get_uniq(m_nodes.front().device);

  // Some devices (e.g. Mayflash adapter) return an empty string which is not very unique.
  if (uniq && std::strlen(uniq) == 0)
    return nullptr;

  return uniq;
}

const char* evdevDevice::GetPhysicalLocation() const
{
  if (m_nodes.empty())
    return nullptr;

  return libevdev_get_phys(m_nodes.front().device);
}

evdevDevice::evdevDevice(InputBackend* input_backend) : m_input_backend(*input_backend)
{
}

evdevDevice::~evdevDevice()
{
  for (auto& node : m_nodes)
  {
    m_input_backend.RemoveDevnodeObject(node.devnode);
    libevdev_free(node.device);
    close(node.fd);
  }
}

void InputBackend::RemoveDevnodeObject(const std::string& node)
{
  m_devnode_objects.erase(node);
}

Core::DeviceRemoval evdevDevice::UpdateInput()
{
  // Run through all evdev events
  // libevdev will keep track of the actual controller state internally which can be queried
  // later with libevdev_fetch_event_value()
  for (auto& node : m_nodes)
  {
    int rc = LIBEVDEV_READ_STATUS_SUCCESS;
    while (rc >= 0)
    {
      input_event ev;
      if (LIBEVDEV_READ_STATUS_SYNC == rc)
        rc = libevdev_next_event(node.device, LIBEVDEV_READ_FLAG_SYNC, &ev);
      else
        rc = libevdev_next_event(node.device, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    }
  }
  return Core::DeviceRemoval::Keep;
}

bool evdevDevice::IsValid() const
{
  for (auto& node : m_nodes)
  {
    const int current_fd = libevdev_get_fd(node.device);

    if (current_fd == -1)
      return false;

    libevdev* device = nullptr;
    if (libevdev_new_from_fd(current_fd, &device) != 0)
    {
      close(current_fd);
      return false;
    }

    libevdev_free(device);
  }

  return true;
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

      static_cast<void>(!write(m_fd, &play, sizeof(play)));
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
