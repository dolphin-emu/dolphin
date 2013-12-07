
#include "Device.h"

#include <string>
#include <sstream>

namespace ciface
{
namespace Core
{

//
//		Device :: ~Device
//
// Destructor, delete all inputs/outputs on device destruction
//
Device::~Device()
{
	{
	// delete inputs
	std::vector<Device::Input*>::iterator
		i = m_inputs.begin(),
		e = m_inputs.end();
	for ( ;i!=e; ++i)
		delete *i;
	}

	{
	// delete outputs
	std::vector<Device::Output*>::iterator
		o = m_outputs.begin(),
		e = m_outputs.end();
	for ( ;o!=e; ++o)
		delete *o;
	}
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
	std::vector<Input*>::const_iterator
		it = m_inputs.begin(),
		itend = m_inputs.end();
	for (; it != itend; ++it)
		if ((*it)->GetName() == name)
			return *it;

	return NULL;
}

Device::Output* Device::FindOutput(const std::string &name) const
{
	std::vector<Output*>::const_iterator
		it = m_outputs.begin(),
		itend = m_outputs.end();
	for (; it != itend; ++it)
		if ((*it)->GetName() == name)
			return *it;

	return NULL;
}

//
//		Device :: ClearInputState
//
// Device classes should override this function
// ControllerInterface will call this when the device returns failure during UpdateInput
// used to try to set all buttons and axes to their default state when user unplugs a gamepad during play
// buttons/axes that were held down at the time of unplugging should be seen as not pressed after unplugging
//
void Device::ClearInputState()
{
	// this is going to be called for every UpdateInput call that fails
	// kinda slow but, w/e, should only happen when user unplugs a device while playing
}

//
//		DeviceQualifier :: ToString
//
// get string from a device qualifier / serialize
//
std::string DeviceQualifier::ToString() const
{
	if (source.empty() && (cid < 0) && name.empty())
		return "";
	std::ostringstream ss;
	ss << source << '/';
	if ( cid > -1 )
		ss << cid;
	ss << '/' << name;
	return ss.str();
}

//
//		DeviceQualifier	::	FromString
//
// set a device qualifier from a string / unserialize
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
//		DeviceQualifier :: FromDevice
//
// set a device qualifier from a device
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
	std::vector<Device*>::const_iterator
		di = m_devices.begin(),
		de = m_devices.end();
	for (; di!=de; ++di)
		if (devq == *di)
			return *di;

	return NULL;
}

Device::Input* DeviceContainer::FindInput(const std::string& name, const Device* def_dev) const
{
	if (def_dev)
	{
		Device::Input* const inp = def_dev->FindInput(name);
		if (inp)
			return inp;
	}

	std::vector<Device*>::const_iterator
		di = m_devices.begin(),
		de = m_devices.end();
	for (; di != de; ++di)
	{
		Device::Input* const i = (*di)->FindInput(name);

		if (i)
			return i;
	}

	return NULL;
}

Device::Output* DeviceContainer::FindOutput(const std::string& name, const Device* def_dev) const
{
	return def_dev->FindOutput(name);
}

}
}
