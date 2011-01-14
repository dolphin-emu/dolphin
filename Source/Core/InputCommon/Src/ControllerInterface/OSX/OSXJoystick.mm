#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>

#include "../ControllerInterface.h"
#include "OSXJoystick.h"

namespace ciface
{
namespace OSX
{

extern void DeviceElementDebugPrint(const void*, void*);

Joystick::Joystick(IOHIDDeviceRef device)
	: m_device(device)
{
	m_device_name = [(NSString *)IOHIDDeviceGetProperty(m_device,
		CFSTR(kIOHIDProductKey)) UTF8String];

	// Buttons
	NSDictionary *buttonDict =
	 [NSDictionary dictionaryWithObjectsAndKeys:
	  [NSNumber numberWithInteger: kIOHIDElementTypeInput_Button],
		@kIOHIDElementTypeKey,
	  [NSNumber numberWithInteger: kHIDPage_Button],
		@kIOHIDElementUsagePageKey,
	  nil];

	CFArrayRef buttons = IOHIDDeviceCopyMatchingElements(m_device,
		(CFDictionaryRef)buttonDict, kIOHIDOptionsTypeNone);

	if (buttons)
	{
		for (int i = 0; i < CFArrayGetCount(buttons); i++)
		{
			IOHIDElementRef e =
			(IOHIDElementRef)CFArrayGetValueAtIndex(buttons, i);
			//DeviceElementDebugPrint(e, NULL);

			AddInput(new Button(e));
		}
		CFRelease(buttons);
	}

	// Axes
	NSDictionary *axisDict =
	[NSDictionary dictionaryWithObjectsAndKeys:
	 [NSNumber numberWithInteger: kIOHIDElementTypeInput_Misc],
		@kIOHIDElementTypeKey,
	 nil];

	CFArrayRef axes = IOHIDDeviceCopyMatchingElements(m_device,
		(CFDictionaryRef)axisDict, kIOHIDOptionsTypeNone);

	if (axes)
	{
		for (int i = 0; i < CFArrayGetCount(axes); i++)
		{
			IOHIDElementRef e =
			(IOHIDElementRef)CFArrayGetValueAtIndex(axes, i);
			//DeviceElementDebugPrint(e, NULL);

			AddInput(new Axis(e, Axis::negative));
			AddInput(new Axis(e, Axis::positive));
		}
		CFRelease(axes);
	}
}

ControlState Joystick::GetInputState(
	const ControllerInterface::Device::Input* const input) const
{
	return ((Input*)input)->GetState(m_device);
}

void Joystick::SetOutputState(
	const ControllerInterface::Device::Output* const output,
	const ControlState state)
{
}

bool Joystick::UpdateInput()
{
	return true;
}

bool Joystick::UpdateOutput()
{
	return true;
}

std::string Joystick::GetName() const
{
	return m_device_name;
}

std::string Joystick::GetSource() const
{
	return "HID";
}

int Joystick::GetId() const
{
	// Overload the "id" to identify devices by HID type when names collide
	// XXX This class is now a catch-all, so query the usage page number
	return kHIDUsage_GD_GamePad;
}


Joystick::Button::Button(IOHIDElementRef element)
	: m_element(element)
{
	std::ostringstream s;
	s << IOHIDElementGetUsage(m_element);
	m_name = std::string("Button ") + s.str();
}

ControlState Joystick::Button::GetState(IOHIDDeviceRef device) const
{
	IOHIDValueRef value;
	if (IOHIDDeviceGetValue(device, m_element, &value) == kIOReturnSuccess)
		return IOHIDValueGetIntegerValue(value);
	else
		return 0;
}

std::string Joystick::Button::GetName() const
{
	return m_name;
}


Joystick::Axis::Axis(IOHIDElementRef element, direction dir)
	: m_element(element)
	, m_direction(dir)
{
	// Need to parse the element a bit first
	std::string description("unk");

	switch (IOHIDElementGetUsage(m_element)) {
	case kHIDUsage_GD_X:
		description = "X";
		break;
	case kHIDUsage_GD_Y:
		description = "Y";
		break;
	case kHIDUsage_GD_Z:
		description = "Z";
		break;
	case kHIDUsage_GD_Rx:
		description = "Rx";
		break;
	case kHIDUsage_GD_Ry:
		description = "Ry";
		break;
	case kHIDUsage_GD_Rz:
		description = "Rz";
		break;
	case kHIDUsage_GD_Wheel:
		description = "Wheel";
		break;
	case kHIDUsage_GD_Hatswitch:
		description = "Hat";
		break;
	case kHIDUsage_Csmr_ACPan:
		description = "Pan";
		break;
	default:
		WARN_LOG(PAD, "Unknown axis type 0x%x, using it anyway...",
			IOHIDElementGetUsage(m_element));
	}

	m_name = std::string("Axis ") + description;
	m_name.append((m_direction == positive) ? "+" : "-");

	m_neutral = (IOHIDElementGetLogicalMax(m_element) -
		IOHIDElementGetLogicalMin(m_element)) / 2.;
}

ControlState Joystick::Axis::GetState(IOHIDDeviceRef device) const
{
	IOHIDValueRef value;

	if (IOHIDDeviceGetValue(device, m_element, &value) == kIOReturnSuccess)
	{
		float position = IOHIDValueGetIntegerValue(value);

		//NSLog(@"%s %f %f", m_name.c_str(), m_neutral, position);

		if (m_direction == positive && position > m_neutral)
			return (position - m_neutral) / m_neutral;
		if (m_direction == negative && position < m_neutral)
			return (m_neutral - position) / m_neutral;
        }
 
        return 0;
}

std::string Joystick::Axis::GetName() const
{
	return m_name;
}


}
}
