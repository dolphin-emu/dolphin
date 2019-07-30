// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/Device.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "Common/StringUtil.h"
#include "Common/Thread.h"

namespace ciface::Core
{
// Compared to an input's current state (ideally 1.0) minus abs(initial_state) (ideally 0.0).
constexpr ControlState INPUT_DETECT_THRESHOLD = 0.55;

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
  return StringFromFormat("%s/%i/%s", this->GetSource().c_str(), GetId(), this->GetName().c_str());
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

ControlState Device::FullAnalogSurface::GetState() const
{
  return (1 + std::max(0.0, m_high.GetState()) - std::max(0.0, m_low.GetState())) / 2;
}

std::string Device::FullAnalogSurface::GetName() const
{
  // E.g. "Full Axis X+"
  return "Full " + m_high.GetName();
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

//
// DeviceQualifier :: ToString
//
// Get string from a device qualifier / serialize
//
std::string DeviceQualifier::ToString() const
{
  if (source.empty() && (cid < 0) && name.empty())
    return "";
  else if (cid > -1)
    return StringFromFormat("%s/%i/%s", source.c_str(), cid, name.c_str());
  else
    return StringFromFormat("%s//%s", source.c_str(), name.c_str());
}

//
// DeviceQualifier :: FromString
//
// Set a device qualifier from a string / unserialize
//
void DeviceQualifier::FromString(const std::string& str)
{
  *this = {};

  // source
  size_t start = 0;
  size_t offset = str.find('/', start);
  if(offset != std::string::npos)
  {
    source = str.substr(start, offset - start);

    // cid
    start = offset + 1;
    offset = str.find('/', start);
    if(offset != std::string::npos && offset - start > 0)
      cid = std::stoi(str.substr(start, offset - start));

    // name
    name = str.substr(offset + 1);
  }
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
  return dev->GetId() == cid && dev->GetName() == name && dev->GetSource() == source;
}

bool DeviceQualifier::operator!=(const Device* const dev) const
{
  return !operator==(dev);
}

bool DeviceQualifier::operator==(const DeviceQualifier& devq) const
{
  return devq.cid == cid && devq.name == name && devq.source == source;
}

bool DeviceQualifier::operator!=(const DeviceQualifier& devq) const
{
  return !operator==(devq);
}

std::shared_ptr<Device> DeviceContainer::FindDevice(const DeviceQualifier& devq) const
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  for (const auto& d : m_devices)
  {
    if (devq == d.get())
      return d;
  }

  return nullptr;
}

std::vector<std::string> DeviceContainer::GetAllDeviceStrings() const
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);

  std::vector<std::string> device_strings;
  DeviceQualifier device_qualifier;

  for (const auto& d : m_devices)
  {
    device_qualifier.FromDevice(d.get());
    device_strings.emplace_back(device_qualifier.ToString());
  }

  return device_strings;
}

std::string DeviceContainer::GetDefaultDeviceString() const
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  if (m_devices.empty())
    return "";

  DeviceQualifier device_qualifier;
  device_qualifier.FromDevice(m_devices[0].get());
  return device_qualifier.ToString();
}

Device::Input* DeviceContainer::FindInput(std::string_view name, const Device* def_dev) const
{
  if (def_dev)
  {
    Device::Input* const inp = def_dev->FindInput(name);
    if (inp)
      return inp;
  }

  std::lock_guard<std::mutex> lk(m_devices_mutex);
  for (const auto& d : m_devices)
  {
    Device::Input* const i = d->FindInput(name);

    if (i)
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

// Wait for input on a particular device.
// Inputs are considered if they are first seen in a neutral state.
// This is useful for crazy flightsticks that have certain buttons that are always held down
// and also properly handles detection when using "FullAnalogSurface" inputs.
// Upon input, return the detected Device and Input, else return nullptrs
std::pair<std::shared_ptr<Device>, Device::Input*>
DeviceContainer::DetectInput(u32 wait_ms, const std::vector<std::string>& device_strings) const
{
  struct InputState
  {
    ciface::Core::Device::Input& input;
    ControlState initial_state;
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
      // Don't detect things like absolute cursor position.
      if (!input->IsDetectable())
        continue;

      // Undesirable axes will have negative values here when trying to map a
      // "FullAnalogSurface".
      input_states.push_back({*input, input->GetState()});
    }

    if (!input_states.empty())
      device_states.emplace_back(DeviceState{std::move(device), std::move(input_states)});
  }

  if (device_states.empty())
    return {};

  u32 time = 0;
  while (time < wait_ms)
  {
    Common::SleepCurrentThread(10);
    time += 10;

    for (auto& device_state : device_states)
    {
      device_state.device->UpdateInput();
      for (auto& input_state : device_state.input_states)
      {
        // We want an input that was initially 0.0 and currently 1.0.
        const auto detection_score =
            (input_state.input.GetState() - std::abs(input_state.initial_state));

        if (detection_score > INPUT_DETECT_THRESHOLD)
          return {device_state.device, &input_state.input};
      }
    }
  }

  // No input was detected. :'(
  return {};
}
}  // namespace ciface::Core
