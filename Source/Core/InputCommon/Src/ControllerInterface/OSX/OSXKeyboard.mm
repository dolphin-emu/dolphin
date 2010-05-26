#include "../ControllerInterface.h"

#ifdef CIFACE_USE_OSX

#include "OSXKeyboard.h"
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


Keyboard::Keyboard(IOHIDDeviceRef device)
	: m_device(device)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	m_device_name = [(NSString *)IOHIDDeviceGetProperty(m_device, CFSTR(kIOHIDProductKey)) UTF8String];
	
	// This class should only recieve Keyboard or Keypad devices
	// Now, filter on just the buttons we can handle sanely
	NSDictionary *matchingElements =
	 [NSDictionary dictionaryWithObjectsAndKeys:
	  [NSNumber numberWithInteger:kIOHIDElementTypeInput_Button], @ kIOHIDElementTypeKey,
	  [NSNumber numberWithInteger:0], @ kIOHIDElementMinKey,
	  [NSNumber numberWithInteger:1], @ kIOHIDElementMaxKey,
	  nil];
	
	CFArrayRef elements =
		IOHIDDeviceCopyMatchingElements(m_device, (CFDictionaryRef)matchingElements, kIOHIDOptionsTypeNone);
	
	if (elements)
	{
		for (int i = 0; i < CFArrayGetCount(elements); i++)
		{
			IOHIDElementRef e = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
			//DeviceElementDebugPrint(e, NULL);
		
			try { inputs.push_back(new Key(e)); }
			catch (std::bad_alloc&) { /*Thrown if the key is reserved*/ }
		}
		CFRelease(elements);
	}
	
	[pool release];
}

ControlState Keyboard::GetInputState( const ControllerInterface::Device::Input* const input )
{
	return ((Input*)input)->GetState(m_device);
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
	return 0;
}


Keyboard::Key::Key(IOHIDElementRef element)
	: m_element(element)
	, m_name("RESERVED") // for some reason HID Manager gives these to us.
{
	uint32_t keycode = IOHIDElementGetUsage(m_element);
	for (uint32_t i = 0; i < sizeof(named_keys)/sizeof(*named_keys); i++)
	{
		if (named_keys[i].code == keycode)
		{
			m_name = named_keys[i].name;
			return;
		}
	}
	NSLog(@"Got key 0x%x of type RESERVED", IOHIDElementGetUsage(m_element));      
}

ControlState Keyboard::Key::GetState(IOHIDDeviceRef device)
{
	IOHIDValueRef value;
	if (IOHIDDeviceGetValue(device, m_element, &value) == kIOReturnSuccess)
		return IOHIDValueGetIntegerValue(value) > 0;

	return false;
}

std::string Keyboard::Key::GetName() const
{
	return m_name;
}


}
}

#endif
