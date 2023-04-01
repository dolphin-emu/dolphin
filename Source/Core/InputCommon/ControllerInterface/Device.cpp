// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>
#include <string>

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
	// delete inputs
	for (Device::Input* input : m_inputs)
		delete input;

	// delete outputs
	for (Device::Output* output: m_outputs)
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

Device::Input* Device::FindInput(const std::string &name) const
{
	for (Input* input : m_inputs)
	{
		if (input->GetName() == name)
			return input;
	}

	return nullptr;
}

Device::Output* Device::FindOutput(const std::string &name) const
{
	for (Output* output : m_outputs)
	{
		if (output->GetName() == name)
			return output;
	}

	return nullptr;
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
	source= dev->GetSource();
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
	if (cid == devq.cid)
		if (name == devq.name)
			if (source == devq.source)
				return true;

	return false;
}

Device* DeviceContainer::FindDevice(const DeviceQualifier& devq) const
{
	for (Device* d : m_devices)
	{
		if (devq == d)
			return d;
	}

	return nullptr;
}

Device::Input* DeviceContainer::FindInput(const std::string& name, const Device* def_dev) const
{
	if (def_dev)
	{
		Device::Input* const inp = def_dev->FindInput(name);
		if (inp)
			return inp;
	}

	for (Device* d : m_devices)
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
