#include "../ControllerInterface.h"

#ifdef CIFACE_USE_OSX

#include "OSX.h"
#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>

namespace ciface
{
namespace OSX
{


struct PrettyKeys
{
	const uint32_t		code;
	const char* const	name;
} named_keys[] =
{
#include "NamedKeys.h"
};

extern void DeviceElementDebugPrint(const void*, void*);


Keyboard::Keyboard(IOHIDDeviceRef device, int index)
	: m_device(device)
	, m_device_index(index)
{
	m_device_name = [(NSString *)IOHIDDeviceGetProperty(m_device, CFSTR(kIOHIDProductKey)) UTF8String];
	
	// Go through all the elements of the device we've been given and try to make something out of them
	CFArrayRef elements = IOHIDDeviceCopyMatchingElements(m_device, NULL, kIOHIDOptionsTypeNone);
	CFIndex idx = 0;
	for (IOHIDElementRef e = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, idx);
		 e;
		 e = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, ++idx))
	{
		if ((IOHIDElementGetType(e) == kIOHIDElementTypeInput_Button) &&
			(IOHIDElementGetUsagePage(e) == kHIDPage_KeyboardOrKeypad) &&
			(IOHIDElementGetLogicalMin(e) == 0) &&
			(IOHIDElementGetLogicalMax(e) == 1))
		{
			try {
				inputs.push_back(new Key(e));
			}
			catch (std::bad_alloc&) {
			}
		}
		else
		{
			//DeviceElementDebugPrint(e, NULL);
		}
	}
	CFRelease(elements);
}

ControlState Keyboard::GetInputState( const ControllerInterface::Device::Input* const input )
{
	return ((Input*)input)->GetState();
}

void Keyboard::SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state )
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
	return "OSX";
}

int Keyboard::GetId() const
{
	return m_device_index;
}


Keyboard::Key::Key(IOHIDElementRef key_element)
	: m_key_element(key_element)
	, m_key_name("RESERVED") // for some reason HID Manager gives these to us. bad_alloc!
{
	uint32_t keycode = IOHIDElementGetUsage(m_key_element);
	for (uint32_t i = 0; i < sizeof(named_keys)/sizeof(*named_keys); i++)
	{
		if (named_keys[i].code == keycode)
		{
			m_key_name = named_keys[i].name;
			return;
		}
	}
	throw std::bad_alloc();
}

ControlState Keyboard::Key::GetState()
{
	IOHIDValueRef value;
	if (IOHIDDeviceGetValue(IOHIDElementGetDevice(m_key_element), m_key_element, &value) == kIOReturnSuccess)
	{
		double scaled_value = IOHIDValueGetScaledValue(value, kIOHIDValueScaleTypePhysical);
		//NSLog(@"element %x value %x scaled %f", IOHIDElementGetUsage(m_key_element), value, scaled_value);
		return scaled_value > 0;
	}

	return false;
}

std::string Keyboard::Key::GetName() const
{
	return m_key_name;
}


}
}

#endif
