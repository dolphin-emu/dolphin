#include "../ControllerInterface.h"

#ifdef CIFACE_USE_OSX

#include "OSXMouse.h"
#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>

namespace ciface
{
namespace OSX
{

extern void DeviceElementDebugPrint(const void*, void*);


Mouse::Mouse(IOHIDDeviceRef device)
	: m_device(device)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	m_device_name = [(NSString *)IOHIDDeviceGetProperty(m_device, CFSTR(kIOHIDProductKey)) UTF8String];
	
	// Buttons
	NSDictionary *buttonDict =
	 [NSDictionary dictionaryWithObjectsAndKeys:
	  [NSNumber numberWithInteger:kIOHIDElementTypeInput_Button], @ kIOHIDElementTypeKey,
	  [NSNumber numberWithInteger:kHIDPage_Button], @ kIOHIDElementUsagePageKey,
	  nil];
	
	CFArrayRef buttons =
		IOHIDDeviceCopyMatchingElements(m_device, (CFDictionaryRef)buttonDict, kIOHIDOptionsTypeNone);
	
	if (buttons)
	{
		for (int i = 0; i < CFArrayGetCount(buttons); i++)
		{
			IOHIDElementRef e = (IOHIDElementRef)CFArrayGetValueAtIndex(buttons, i);
			//DeviceElementDebugPrint(e, NULL);
		
			inputs.push_back(new Button(e));
		}
		CFRelease(buttons);
	}
	
	// Axes
	NSDictionary *axisDict =
	[NSDictionary dictionaryWithObjectsAndKeys:
	 [NSNumber numberWithInteger:kIOHIDElementTypeInput_Misc], @ kIOHIDElementTypeKey,
	 nil];
	
	CFArrayRef axes =
		IOHIDDeviceCopyMatchingElements(m_device, (CFDictionaryRef)axisDict, kIOHIDOptionsTypeNone);
	
	if (axes)
	{
		for (int i = 0; i < CFArrayGetCount(axes); i++)
		{
			IOHIDElementRef e = (IOHIDElementRef)CFArrayGetValueAtIndex(axes, i);
			//DeviceElementDebugPrint(e, NULL);
			
			inputs.push_back(new Axis(e, Axis::negative));
			inputs.push_back(new Axis(e, Axis::positive));
		}
		CFRelease(axes);
	}
	
	[pool release];
}

ControlState Mouse::GetInputState( const ControllerInterface::Device::Input* const input )
{
	return ((Input*)input)->GetState();
}

void Mouse::SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state )
{
}

bool Mouse::UpdateInput()
{
	return true;
}

bool Mouse::UpdateOutput()
{
	return true;
}

std::string Mouse::GetName() const
{
	return m_device_name;
}

std::string Mouse::GetSource() const
{
	return "OSX";
}

int Mouse::GetId() const
{
	return 0;
}


Mouse::Button::Button(IOHIDElementRef element)
	: m_element(element)
{
	m_device = IOHIDElementGetDevice(m_element);
	
	std::ostringstream s;
	s << IOHIDElementGetUsage(m_element);
	m_name = std::string("Button ") + s.str();
}

ControlState Mouse::Button::GetState()
{
	IOHIDValueRef value;
	if (IOHIDDeviceGetValue(m_device, m_element, &value) == kIOReturnSuccess)
		return IOHIDValueGetScaledValue(value, kIOHIDValueScaleTypePhysical) > 0;

	return false;
}

std::string Mouse::Button::GetName() const
{
	return m_name;
}


Mouse::Axis::Axis(IOHIDElementRef element, direction dir)
	: m_element(element)
	, m_direction(dir)
{
	m_device = IOHIDElementGetDevice(m_element);

	// Need to parse the element a bit first
	std::string description("unk");
	
	switch (IOHIDElementGetUsage(m_element))
	{
		default:
			NSLog(@"Unknown axis type 0x%x, using anyways...", IOHIDElementGetUsage(m_element));
			break;
		case kHIDUsage_GD_X:		description = "X";		break;
		case kHIDUsage_GD_Y:		description = "Y";		break;
		case kHIDUsage_GD_Wheel:	description = "Wheel";	break;
		case kHIDUsage_Csmr_ACPan:	description = "Pan";	break;
	}
	
	m_name = std::string("Axis ") + description;
	m_name.append((m_direction == positive) ? std::string("+") : std::string("-"));
}

ControlState Mouse::Axis::GetState()
{
	IOHIDValueRef value;
	if (IOHIDDeviceGetValue(m_device, m_element, &value) == kIOReturnSuccess)	
	{
		double scaled_value = IOHIDValueGetScaledValue(value, kIOHIDValueScaleTypePhysical);
		double actual_value = 0;
		
		if ((scaled_value < 0) && (m_direction == negative))
			actual_value = fabs(scaled_value);
		else if ((scaled_value > 0) && (m_direction == positive))
			actual_value = scaled_value;
		
		//NSLog(@"%s %f %f", m_name.c_str(), scaled_value, actual_value);
		return actual_value;
	}
	
	return false;
}

std::string Mouse::Axis::GetName() const
{
	return m_name;
}


}
}

#endif
