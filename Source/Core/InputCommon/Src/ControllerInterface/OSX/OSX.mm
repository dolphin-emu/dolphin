#include "../ControllerInterface.h"

#ifdef CIFACE_USE_OSX

#include "OSX.h"
#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>

namespace ciface
{
namespace OSX
{

struct PrettyLights
{
	const uint32_t		code;
	const char* const	name;
} named_lights[] =
{
	{ kHIDUsage_LED_NumLock, "Num Lock" },
	{ kHIDUsage_LED_CapsLock, "Caps Lock" },
	{ kHIDUsage_LED_ScrollLock, "Scroll Lock" },
	{ kHIDUsage_LED_Compose, "Compose" },
	{ kHIDUsage_LED_Kana, "Kana" },
	{ kHIDUsage_LED_Power, "Power" }
};

static IOHIDManagerRef HIDManager = NULL;
static CFStringRef OurRunLoop = CFSTR("DolphinOSXInput");


static void DeviceMatching_callback(void* inContext,
									IOReturn inResult,
									void* inSender,
									IOHIDDeviceRef inIOHIDDeviceRef)
{
	NSLog(@"-------------------------");
	NSLog(@"Got Device: %@", IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDProductKey)));
	
	// Add to the devices vector if it's of a type we want
	if (IOHIDDeviceConformsTo(inIOHIDDeviceRef, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) ||
		IOHIDDeviceConformsTo(inIOHIDDeviceRef, kHIDPage_GenericDesktop, kHIDUsage_GD_Keypad)/* ||
		IOHIDDeviceConformsTo(inIOHIDDeviceRef, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse)*/)
	{
		std::vector<ControllerInterface::Device*> *devices = (std::vector<ControllerInterface::Device*> *)inContext;
		devices->push_back(new KeyboardMouse(inIOHIDDeviceRef));
	}
	else
	{
		// Actually, we don't want it
		NSLog(@"Throwing away...");
		#define shortlog(x)
		//#define shortlog(x) NSLog(@"%s: %@", x, IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(x)));
		shortlog(kIOHIDTransportKey)
		shortlog(kIOHIDVendorIDKey)
		shortlog(kIOHIDVendorIDSourceKey)
		shortlog(kIOHIDProductIDKey)
		shortlog(kIOHIDVersionNumberKey)
		shortlog(kIOHIDManufacturerKey)
		shortlog(kIOHIDProductKey)
		shortlog(kIOHIDSerialNumberKey)
		shortlog(kIOHIDCountryCodeKey)
		shortlog(kIOHIDLocationIDKey)
		shortlog(kIOHIDDeviceUsageKey)
		shortlog(kIOHIDDeviceUsagePageKey)
		shortlog(kIOHIDDeviceUsagePairsKey)
		shortlog(kIOHIDPrimaryUsageKey)
		shortlog(kIOHIDPrimaryUsagePageKey)
		shortlog(kIOHIDMaxInputReportSizeKey)
		shortlog(kIOHIDMaxOutputReportSizeKey)
		shortlog(kIOHIDMaxFeatureReportSizeKey)
		shortlog(kIOHIDReportIntervalKey)
		shortlog(kIOHIDReportDescriptorKey)
		#undef shortlog
	}
}

void Init( std::vector<ControllerInterface::Device*>& devices )
{
	HIDManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (!HIDManager)
		NSLog(@"Failed to create HID Manager reference");
	
	// HID Manager will give us the following devices:
	// Keyboard, Keypad, Mouse, GamePad
	NSArray *matchingDevices = [NSArray arrayWithObjects:
		[NSDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithInteger:kHIDPage_GenericDesktop], @ kIOHIDDeviceUsagePageKey,
			[NSNumber numberWithInteger:kHIDUsage_GD_Keyboard], @ kIOHIDDeviceUsageKey, nil],
		[NSDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithInteger:kHIDPage_GenericDesktop], @ kIOHIDDeviceUsagePageKey,
			[NSNumber numberWithInteger:kHIDUsage_GD_Keypad], @ kIOHIDDeviceUsageKey, nil],
		[NSDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithInteger:kHIDPage_GenericDesktop], @ kIOHIDDeviceUsagePageKey,
			[NSNumber numberWithInteger:kHIDUsage_GD_Mouse], @ kIOHIDDeviceUsageKey, nil],
		[NSDictionary dictionaryWithObjectsAndKeys:
			[NSNumber numberWithInteger:kHIDPage_GenericDesktop], @ kIOHIDDeviceUsagePageKey,
			[NSNumber numberWithInteger:kHIDUsage_GD_GamePad], @ kIOHIDDeviceUsageKey, nil],
		nil];
	// Pass NULL to get all devices
	IOHIDManagerSetDeviceMatchingMultiple(HIDManager, (CFArrayRef)matchingDevices);
	
	// Callbacks for acquisition or loss of a matching device
	IOHIDManagerRegisterDeviceMatchingCallback(HIDManager, DeviceMatching_callback, (void *)&devices);

	// Match devices that are plugged right now.
	IOHIDManagerScheduleWithRunLoop(HIDManager, CFRunLoopGetCurrent(), OurRunLoop);
	if (IOHIDManagerOpen(HIDManager, kIOHIDOptionsTypeNone) != kIOReturnSuccess)
		NSLog(@"Failed to open HID Manager");
	
	// Wait while current devices are initialized
	while (CFRunLoopRunInMode(OurRunLoop,0,TRUE) == kCFRunLoopRunHandledSource);
	
	// Things should be configured now. Disable hotplugging and other scheduling
	IOHIDManagerRegisterDeviceMatchingCallback(HIDManager, NULL, NULL);
    IOHIDManagerUnscheduleFromRunLoop(HIDManager, CFRunLoopGetCurrent(), OurRunLoop);
}

void DeInit()
{
	// This closes all devices as well
	IOHIDManagerClose(HIDManager, kIOHIDOptionsTypeNone);
	CFRelease(HIDManager);
}

	
void DeviceElementDebugPrint(const void *value, void *context)
{
	IOHIDElementRef e = (IOHIDElementRef)value;
	
	std::string type = "";
	switch (IOHIDElementGetType(e))
	{
	case kIOHIDElementTypeInput_Axis: type = "axis"; break;
	case kIOHIDElementTypeInput_Button: type = "button"; break;
	case kIOHIDElementTypeInput_Misc: type = "misc"; break;
	case kIOHIDElementTypeInput_ScanCodes: type = "scancodes"; break;
	case kIOHIDElementTypeOutput: type = "output"; break;
	case kIOHIDElementTypeFeature: type = "feature"; break;
	case kIOHIDElementTypeCollection: type = "collection"; break;
	}
	
	std::string c_type = "";
	if (type == "collection")
	{
		switch (IOHIDElementGetCollectionType(e))
		{
		case kIOHIDElementCollectionTypePhysical: c_type = "physical"; break;
		case kIOHIDElementCollectionTypeApplication: c_type = "application"; break;
		case kIOHIDElementCollectionTypeLogical: c_type = "logical"; break;
		case kIOHIDElementCollectionTypeReport: c_type = "report"; break;
		case kIOHIDElementCollectionTypeNamedArray: c_type = "namedArray"; break;
		case kIOHIDElementCollectionTypeUsageSwitch: c_type = "usageSwitch"; break;
		case kIOHIDElementCollectionTypeUsageModifier: c_type = "usageModifier"; break;
		}
	}
	
	c_type.append(" ");
	NSLog(@"%s%s%spage: 0x%x usage: 0x%x name: %s lmin: %u lmax: %u",
		  type.c_str(),
		  type == "collection" ? ":" : "",
		  type == "collection" ? c_type.c_str() : " ",
		  IOHIDElementGetUsagePage(e),
		  IOHIDElementGetUsage(e),
		  IOHIDElementGetName(e), // TOO BAD IT"S FUCKING USELESS
		  IOHIDElementGetLogicalMin(e),
		  IOHIDElementGetLogicalMax(e));
	
	if (type == "collection")
	{
		CFArrayRef elements = IOHIDElementGetChildren(e);
		CFRange range = {0, CFArrayGetCount(elements)};
		// this leaks...but it's just debug code, right? :D
		CFArrayApplyFunction(elements, range, DeviceElementDebugPrint, NULL);
	}
}

	
KeyboardMouse::KeyboardMouse(IOHIDDeviceRef device)
	: m_device(device)
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
			inputs.push_back(new Key(e));
		}
		else if((IOHIDElementGetType(e) == kIOHIDElementTypeOutput) &&
				(IOHIDElementGetUsagePage(e) == kHIDPage_LEDs) &&
				(IOHIDElementGetLogicalMin(e) == 0) &&
				(IOHIDElementGetLogicalMax(e) == 1))
		{
			outputs.push_back(new Light(e));
		}
		else
		{
//			DeviceElementDebugPrint(e, NULL);
		}

	}
	CFRelease(elements);
}

ControlState KeyboardMouse::GetInputState( const ControllerInterface::Device::Input* const input )
{
	return ((Input*)input)->GetState( &m_state_in );
}

void KeyboardMouse::SetOutputState( const ControllerInterface::Device::Output* const output, const ControlState state )
{
	((Output*)output)->SetState(state, m_state_out);
}

bool KeyboardMouse::UpdateInput()
{
	return true;
}

bool KeyboardMouse::UpdateOutput()
{
	return true;
}

std::string KeyboardMouse::GetName() const
{
	return m_device_name;
}

std::string KeyboardMouse::GetSource() const
{
	return "OSX";
}

int KeyboardMouse::GetId() const
{
	return 0;
}


KeyboardMouse::Key::Key(IOHIDElementRef key_element)
	: m_key_element(key_element)
{
	std::ostringstream s;
	s << IOHIDElementGetUsage(m_key_element);
	m_key_name = s.str();
}

ControlState KeyboardMouse::Key::GetState( const State* const state )
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

std::string KeyboardMouse::Key::GetName() const
{
	return m_key_name;
}


KeyboardMouse::Button::Button(IOHIDElementRef button_element)
: m_button_element(button_element)
{
	std::ostringstream s;
	s << IOHIDElementGetUsage(m_button_element);
	m_button_name = s.str();
}

ControlState KeyboardMouse::Button::GetState( const State* const state )
{
	return false;
}

std::string KeyboardMouse::Button::GetName() const
{
	return m_button_name;
}


KeyboardMouse::Axis::Axis(IOHIDElementRef axis_element)
: m_axis_element(axis_element)
{
	std::ostringstream s;
	s << IOHIDElementGetUsage(m_axis_element);
	m_axis_name = s.str();
}

ControlState KeyboardMouse::Axis::GetState( const State* const state )
{
	return false;
}

std::string KeyboardMouse::Axis::GetName() const
{
	return m_axis_name;
}


KeyboardMouse::Light::Light(IOHIDElementRef light_element)
: m_light_element(light_element)
{
	int idx = IOHIDElementGetUsage(m_light_element);
	m_light_name = (idx <= 5) ? named_lights[idx].name : "UNKNOWN";
}

void KeyboardMouse::Light::SetState( const ControlState state, unsigned char* const state_out )
{
	uint64_t timestamp = 0;
	IOHIDValueRef value = IOHIDValueCreateWithIntegerValue(kCFAllocatorDefault, m_light_element, timestamp, (int)state);
	if (IOHIDDeviceSetValue(IOHIDElementGetDevice(m_light_element), m_light_element, value) == kIOReturnSuccess)
	{
		NSLog(@"element %x value %x", IOHIDElementGetUsage(m_light_element), value);
	}
}

std::string KeyboardMouse::Light::GetName() const
{
	return m_light_name;
}


}
}

#endif
