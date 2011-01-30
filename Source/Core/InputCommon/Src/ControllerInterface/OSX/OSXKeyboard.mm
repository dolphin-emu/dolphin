#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>

#include "../ControllerInterface.h"
#include "OSXKeyboard.h"

namespace ciface
{
namespace OSX
{


const struct PrettyKeys
{
	const uint32_t		code;
	const char* const	name;
} named_keys[] =
{
#include "NamedKeys.h"
};

extern void DeviceElementDebugPrint(const void *, void *);

Keyboard::Keyboard(IOHIDDeviceRef device, std::string name, int index)
	: m_device(device)
	, m_device_name(name)
	, m_index(index)
{
	// This class should only recieve Keyboard or Keypad devices
	// Now, filter on just the buttons we can handle sanely
	NSDictionary *matchingElements =
	 [NSDictionary dictionaryWithObjectsAndKeys:
	  [NSNumber numberWithInteger: kIOHIDElementTypeInput_Button],
		@kIOHIDElementTypeKey,
	  [NSNumber numberWithInteger: 0], @kIOHIDElementMinKey,
	  [NSNumber numberWithInteger: 1], @kIOHIDElementMaxKey,
	  nil];

	CFArrayRef elements = IOHIDDeviceCopyMatchingElements(m_device,
		(CFDictionaryRef)matchingElements, kIOHIDOptionsTypeNone);

	if (elements)
	{
		for (int i = 0; i < CFArrayGetCount(elements); i++)
		{
			IOHIDElementRef e =
			(IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
			//DeviceElementDebugPrint(e, NULL);

			AddInput(new Key(e));
		}
		CFRelease(elements);
	}
}

ControlState Keyboard::GetInputState(
	const ControllerInterface::Device::Input* const input) const
{
	return ((Input*)input)->GetState(m_device);
}

void Keyboard::SetOutputState(
	const ControllerInterface::Device::Output * const output,
	const ControlState state)
{
}

bool Keyboard::UpdateInput()
{
	return true;
}

bool Keyboard::UpdateOutput()
{
	return true;
}

std::string Keyboard::GetName() const
{
	return m_device_name;
}

std::string Keyboard::GetSource() const
{
	return "Keyboard";
}

int Keyboard::GetId() const
{
	return m_index;
}

Keyboard::Key::Key(IOHIDElementRef element)
	: m_element(element)
{
	uint32_t i, keycode;

	m_name = "RESERVED";
	keycode = IOHIDElementGetUsage(m_element);
	for (i = 0; i < sizeof named_keys / sizeof *named_keys; i++)
		if (named_keys[i].code == keycode)
			m_name = named_keys[i].name;
}

ControlState Keyboard::Key::GetState(IOHIDDeviceRef device) const
{
	IOHIDValueRef value;

	if (IOHIDDeviceGetValue(device, m_element, &value) == kIOReturnSuccess)
		return IOHIDValueGetIntegerValue(value);
	else
		return 0;
}

std::string Keyboard::Key::GetName() const
{
	return m_name;
}


}
}
