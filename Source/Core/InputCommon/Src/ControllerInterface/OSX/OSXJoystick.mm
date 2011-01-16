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

			if (IOHIDElementGetUsage(e) == kHIDUsage_GD_Hatswitch) {
				AddInput(new Hat(e, Hat::up));
				AddInput(new Hat(e, Hat::right));
				AddInput(new Hat(e, Hat::down));
				AddInput(new Hat(e, Hat::left));
			} else {
				AddInput(new Axis(e, Axis::negative));
				AddInput(new Axis(e, Axis::positive));
			}
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
	case kHIDUsage_Csmr_ACPan:
		description = "Pan";
		break;
	}

	m_name = std::string("Axis ") + description;
	m_name.append((m_direction == positive) ? "+" : "-");

	m_neutral = (IOHIDElementGetLogicalMax(m_element) +
		IOHIDElementGetLogicalMin(m_element)) / 2.;
	m_scale = 1 / fabs(IOHIDElementGetLogicalMax(m_element) - m_neutral);
}

ControlState Joystick::Axis::GetState(IOHIDDeviceRef device) const
{
	IOHIDValueRef value;

	if (IOHIDDeviceGetValue(device, m_element, &value) == kIOReturnSuccess)
	{
		float position = IOHIDValueGetIntegerValue(value);

		if (m_direction == positive && position > m_neutral)
			return (position - m_neutral) * m_scale;
		if (m_direction == negative && position < m_neutral)
			return (m_neutral - position) * m_scale;
	}

	return 0;
}

std::string Joystick::Axis::GetName() const
{
	return m_name;
}

Joystick::Hat::Hat(IOHIDElementRef element, direction dir)
	: m_element(element)
	, m_direction(dir)
{
	switch (dir) {
	case up:
		m_name = "Up";
		break;
	case right:
		m_name = "Right";
		break;
	case down:
		m_name = "Down";
		break;
	case left:
		m_name = "Left";
		break;
	default:
		m_name = "unk";
	}
}

ControlState Joystick::Hat::GetState(IOHIDDeviceRef device) const
{
	IOHIDValueRef value;
	int position;

	if (IOHIDDeviceGetValue(device, m_element, &value) == kIOReturnSuccess)
	{
		position = IOHIDValueGetIntegerValue(value);

		switch (position) {
		case 0:
			if (m_direction == up)
				return 1;
			break;
		case 1:
			if (m_direction == up || m_direction == right)
				return 1;
			break;
		case 2:
			if (m_direction == right)
				return 1;
			break;
		case 3:
			if (m_direction == right || m_direction == down)
				return 1;
			break;
		case 4:
			if (m_direction == down)
				return 1;
			break;
		case 5:
			if (m_direction == down || m_direction == left)
				return 1;
			break;
		case 6:
			if (m_direction == left)
				return 1;
			break;
		case 7:
			if (m_direction == left || m_direction == up)
				return 1;
			break;
		};
	}

	return 0;
}

std::string Joystick::Hat::GetName() const
{
	return m_name;
}


}
}
