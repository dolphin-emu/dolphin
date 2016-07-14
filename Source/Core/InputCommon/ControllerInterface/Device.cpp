// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>

// For InputGateOn()
// This is a really bad layering violation, but it's the cleanest
// place I could find to put it.
#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace Core
{
//
// Device :: ~Device
//
// Destructor, delete all inputs/outputs on device destruction
//
Device::~Device()
{
}

void Device::AddInput(std::unique_ptr<Input> input)
{
  m_inputs.push_back(std::move(input));
}

void Device::AddOutput(std::unique_ptr<Output> output)
{
  m_outputs.push_back(std::move(output));
}

Device::Input* Device::FindInput(const std::string& name) const
{
  const auto iter = std::find_if(m_inputs.begin(), m_inputs.end(),
                                 [&name](const auto& input) { return input->GetName() == name; });

  if (iter == m_inputs.end())
    return nullptr;

  return iter->get();
}

Device::Output* Device::FindOutput(const std::string& name) const
{
  const auto iter = std::find_if(m_outputs.begin(), m_outputs.end(),
                                 [&name](const auto& output) { return output->GetName() == name; });

  if (iter == m_outputs.end())
    return nullptr;

  return iter->get();
}

bool Device::Control::InputGateOn()
{
  if (SConfig::GetInstance().m_BackgroundInput)
    return true;
  else if (Host_RendererHasFocus() || Host_UIHasFocus())
    return true;
  else
    return false;
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

  std::ostringstream ss;
  ss << source << '/';
  if (cid > -1)
    ss << cid;
  ss << '/' << name;

  return ss.str();
}

//
// DeviceQualifier :: FromString
//
// Set a device qualifier from a string / unserialize
//
void DeviceQualifier::FromString(const std::string& str)
{
  std::istringstream ss(str);

  std::getline(ss, source = "", '/');

  // silly
  std::getline(ss, name, '/');
  std::istringstream(name) >> (cid = -1);

  std::getline(ss, name = "");
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

bool DeviceQualifier::operator==(const DeviceQualifier& devq) const
{
  return std::tie(cid, name, source) == std::tie(devq.cid, devq.name, devq.source);
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

Device::Input* DeviceContainer::FindInput(const std::string& name, const Device* def_dev) const
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

Device::Output* DeviceContainer::FindOutput(const std::string& name, const Device* def_dev) const
{
  return def_dev->FindOutput(name);
}
}
}
