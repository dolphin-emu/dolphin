// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <string>

#include "Common/StringUtil.h"
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
  // delete inputs
  for (Device::Input* input : m_inputs)
    delete input;

  // delete outputs
  for (Device::Output* output : m_outputs)
    delete output;
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

Device::Input* Device::FindInput(const std::string& name) const
{
  for (Input* input : m_inputs)
  {
    if (input->GetName() == name)
      return input;
  }

  return nullptr;
}

Device::Output* Device::FindOutput(const std::string& name) const
{
  for (Output* output : m_outputs)
  {
    if (output->GetName() == name)
      return output;
  }
  return nullptr;
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
      cid = std::stoi(str.substr(start, offset - start)) % 4;

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

bool DeviceContainer::HasConnectedDevice(const DeviceQualifier& qualifier) const
{
  const auto device = FindDevice(qualifier);
  return device != nullptr && device->IsValid();
}
}
}
