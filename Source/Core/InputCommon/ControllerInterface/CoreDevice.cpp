// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/CoreDevice.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>

#include <fmt/format.h>

#include "Common/MathUtil.h"
#include "Common/Thread.h"

namespace ciface::Core
{
// Compared to an input's current state (ideally 1.0) minus abs(initial_state) (ideally 0.0).
// Note: Detect() logic assumes this is greater than 0.5.
constexpr ControlState INPUT_DETECT_THRESHOLD = 0.55;

class CombinedInput final : public Device::Input
{
public:
  using Inputs = std::pair<Device::Input*, Device::Input*>;

  CombinedInput(std::string name, const Inputs& inputs) : m_name(std::move(name)), m_inputs(inputs)
  {
  }
  ControlState GetState() const override
  {
    ControlState result = 0;

    if (m_inputs.first)
      result = m_inputs.first->GetState();

    if (m_inputs.second)
      result = std::max(result, m_inputs.second->GetState());

    return result;
  }
  std::string GetName() const override { return m_name; }
  bool IsDetectable() const override { return false; }
  bool IsChild(const Input* input) const override
  {
    return m_inputs.first == input || m_inputs.second == input;
  }

private:
  const std::string m_name;
  const std::pair<Device::Input*, Device::Input*> m_inputs;
};

Device::~Device()
{
  // delete inputs
  for (Device::Input* input : m_inputs)
    delete input;

  // delete outputs
  for (Device::Output* output : m_outputs)
    delete output;
}

std::optional<int> Device::GetPreferredId() const
{
  return {};
}

void Device::AddInput(Device::Input* const i)
{
  m_inputs.push_back(i);
}

void Device::AddOutput(Device::Output* const o)
{
  m_outputs.push_back(o);
}

std::string Device::GetQualifiedName() const
{
  return fmt::format("{}/{}/{}", GetSource(), GetId(), GetName());
}

auto Device::GetParentMostInput(Input* child) const -> Input*
{
  for (auto* input : m_inputs)
  {
    if (input->IsChild(child))
    {
      // Running recursively is currently unnecessary but it doesn't hurt.
      return GetParentMostInput(input);
    }
  }

  return child;
}

Device::Input* Device::FindInput(std::string_view name) const
{
  for (Input* input : m_inputs)
  {
    if (input->IsMatchingName(name))
      return input;
  }

  return nullptr;
}

Device::Output* Device::FindOutput(std::string_view name) const
{
  for (Output* output : m_outputs)
  {
    if (output->IsMatchingName(name))
      return output;
  }

  return nullptr;
}

bool Device::Control::IsMatchingName(std::string_view name) const
{
  return GetName() == name;
}

bool Device::Control::IsHidden() const
{
  return false;
}

ControlState Device::FullAnalogSurface::GetState() const
{
  return (1 + std::max(0.0, m_high.GetState()) - std::max(0.0, m_low.GetState())) / 2;
}

std::string Device::FullAnalogSurface::GetName() const
{
  // E.g. "Full Axis X+"
  return "Full " + m_high.GetName();
}

bool Device::FullAnalogSurface::IsDetectable() const
{
  return m_low.IsDetectable() && m_high.IsDetectable();
}

bool Device::FullAnalogSurface::IsHidden() const
{
  return m_low.IsHidden() && m_high.IsHidden();
}

bool Device::FullAnalogSurface::IsMatchingName(std::string_view name) const
{
  if (Control::IsMatchingName(name))
    return true;

  // Old naming scheme was "Axis X-+" which is too visually similar to "Axis X+".
  // This has caused countless problems for users with mysterious misconfigurations.
  // We match this old name to support old configurations.
  const auto old_name = m_low.GetName() + *m_high.GetName().rbegin();

  return old_name == name;
}

void Device::AddCombinedInput(std::string name, const std::pair<std::string, std::string>& inputs)
{
  AddInput(new CombinedInput(std::move(name), {FindInput(inputs.first), FindInput(inputs.second)}));
}

//
// DeviceQualifier :: ToString
//
// Get string from a device qualifier / serialize
//
std::string DeviceQualifier::ToString() const
{
  if (source.empty() && (cid < 0) && name.empty())
    return {};

  if (cid > -1)
    return fmt::format("{}/{}/{}", source, cid, name);
  else
    return fmt::format("{}//{}", source, name);
}

//
// DeviceQualifier :: FromString
//
// Set a device qualifier from a string / unserialize
//
void DeviceQualifier::FromString(const std::string& str)
{
  *this = {};

  std::istringstream ss(str);

  std::getline(ss, source, '/');

  // silly
  std::getline(ss, name, '/');
  std::istringstream(name) >> cid;

  std::getline(ss, name);
}

//
// DeviceQualifier :: FromDevice
//
// Set a device qualifier from a device
//
void DeviceQualifier::FromDevice(const Device* const dev)
{
  name = dev->GetName();
  cid = dev->GetId();
  source = dev->GetSource();
}

bool DeviceQualifier::operator==(const Device* const dev) const
{
  if (dev->GetId() == cid)
    if (dev->GetName() == name)
      if (dev->GetSource() == source)
        return true;

  return false;
}

bool DeviceQualifier::operator!=(const Device* const dev) const
{
  return !operator==(dev);
}

bool DeviceQualifier::operator==(const DeviceQualifier& devq) const
{
  return std::tie(cid, name, source) == std::tie(devq.cid, devq.name, devq.source);
}

bool DeviceQualifier::operator!=(const DeviceQualifier& devq) const
{
  return !operator==(devq);
}

std::shared_ptr<Device> DeviceContainer::FindDevice(const DeviceQualifier& devq) const
{
  std::lock_guard lk(m_devices_mutex);
  for (const auto& d : m_devices)
  {
    if (devq == d.get())
      return d;
  }

  return nullptr;
}

std::vector<std::shared_ptr<Device>> DeviceContainer::GetAllDevices() const
{
  std::lock_guard lk(m_devices_mutex);

  std::vector<std::shared_ptr<Device>> devices;

  for (const auto& d : m_devices)
    devices.emplace_back(d);

  return devices;
}

std::vector<std::string> DeviceContainer::GetAllDeviceStrings() const
{
  std::lock_guard lk(m_devices_mutex);

  std::vector<std::string> device_strings;
  DeviceQualifier device_qualifier;

  for (const auto& d : m_devices)
  {
    device_qualifier.FromDevice(d.get());
    device_strings.emplace_back(device_qualifier.ToString());
  }

  return device_strings;
}

bool DeviceContainer::HasDefaultDevice() const
{
  std::lock_guard lk(m_devices_mutex);
  // Devices are already sorted by priority
  return !m_devices.empty() && m_devices[0]->GetSortPriority() >= 0;
}

std::string DeviceContainer::GetDefaultDeviceString() const
{
  std::lock_guard lk(m_devices_mutex);
  // Devices are already sorted by priority
  if (m_devices.empty() || m_devices[0]->GetSortPriority() < 0)
    return "";

  DeviceQualifier device_qualifier;
  device_qualifier.FromDevice(m_devices[0].get());
  return device_qualifier.ToString();
}

Device::Input* DeviceContainer::FindInput(std::string_view name, const Device* def_dev) const
{
  if (def_dev)
  {
    if (Device::Input* const inp = def_dev->FindInput(name))
      return inp;
  }

  std::lock_guard lk(m_devices_mutex);
  for (const auto& d : m_devices)
  {
    if (Device::Input* const i = d->FindInput(name))
      return i;
  }

  return nullptr;
}

Device::Output* DeviceContainer::FindOutput(std::string_view name, const Device* def_dev) const
{
  return def_dev->FindOutput(name);
}

bool DeviceContainer::HasConnectedDevice(const DeviceQualifier& qualifier) const
{
  const auto device = FindDevice(qualifier);
  return device != nullptr && device->IsValid();
}

// Wait for inputs on supplied devices.
// Inputs are only considered if they are first seen in a neutral state.
// This is useful for crazy flightsticks that have certain buttons that are always held down
// and also properly handles detection when using "FullAnalogSurface" inputs.
// Multiple detections are returned until the various timeouts have been reached.
auto DeviceContainer::DetectInput(const std::vector<std::string>& device_strings,
                                  std::chrono::milliseconds initial_wait,
                                  std::chrono::milliseconds confirmation_wait,
                                  std::chrono::milliseconds maximum_wait) const
    -> std::vector<InputDetection>
{
  struct InputState
  {
    InputState(ciface::Core::Device::Input* input_) : input{input_} { stats.Push(0.0); }

    ciface::Core::Device::Input* input;
    ControlState initial_state = input->GetState();
    ControlState last_state = initial_state;
    MathUtil::RunningVariance<ControlState> stats;

    // Prevent multiiple detections until after release.
    bool is_ready = true;

    void Update()
    {
      const auto new_state = input->GetState();

      if (!is_ready && new_state < (1 - INPUT_DETECT_THRESHOLD))
      {
        last_state = new_state;
        is_ready = true;
        stats.Clear();
      }

      const auto difference = new_state - last_state;
      stats.Push(difference);
      last_state = new_state;
    }

    bool IsPressed()
    {
      if (!is_ready)
        return false;

      // We want an input that was initially 0.0 and currently 1.0.
      const auto detection_score = (last_state - std::abs(initial_state));
      return detection_score > INPUT_DETECT_THRESHOLD;
    }
  };

  struct DeviceState
  {
    std::shared_ptr<Device> device;

    std::vector<InputState> input_states;
  };

  // Acquire devices and initial input states.
  std::vector<DeviceState> device_states;
  for (const auto& device_string : device_strings)
  {
    DeviceQualifier dq;
    dq.FromString(device_string);
    auto device = FindDevice(dq);

    if (!device)
      continue;

    std::vector<InputState> input_states;

    for (auto* input : device->Inputs())
    {
      // Don't detect things like absolute cursor positions, accelerometers, or gyroscopes.
      if (!input->IsDetectable())
        continue;

      // Undesirable axes will have negative values here when trying to map a
      // "FullAnalogSurface".
      input_states.push_back(InputState{input});
    }

    if (!input_states.empty())
      device_states.emplace_back(DeviceState{std::move(device), std::move(input_states)});
  }

  if (device_states.empty())
    return {};

  std::vector<InputDetection> detections;

  const auto start_time = Clock::now();
  while (true)
  {
    const auto now = Clock::now();
    const auto elapsed_time = now - start_time;

    if (elapsed_time >= maximum_wait || (detections.empty() && elapsed_time >= initial_wait) ||
        (!detections.empty() && detections.back().release_time.has_value() &&
         now >= *detections.back().release_time + confirmation_wait))
    {
      break;
    }

    Common::SleepCurrentThread(10);

    for (auto& device_state : device_states)
    {
      for (std::size_t i = 0; i != device_state.input_states.size(); ++i)
      {
        auto& input_state = device_state.input_states[i];
        input_state.Update();

        if (input_state.IsPressed())
        {
          input_state.is_ready = false;

          // Digital presses will evaluate as 1 here.
          // Analog presses will evaluate greater than 1.
          const auto smoothness =
              1 / std::sqrt(input_state.stats.Variance() / input_state.stats.Mean());

          InputDetection new_detection;
          new_detection.device = device_state.device;
          new_detection.input = input_state.input;
          new_detection.press_time = Clock::now();
          new_detection.smoothness = smoothness;

          // We found an input. Add it to our detections.
          detections.emplace_back(std::move(new_detection));
        }
      }
    }

    // Check for any releases of our detected inputs.
    for (auto& d : detections)
    {
      if (!d.release_time.has_value() && d.input->GetState() < (1 - INPUT_DETECT_THRESHOLD))
        d.release_time = Clock::now();
    }
  }

  return detections;
}
}  // namespace ciface::Core
