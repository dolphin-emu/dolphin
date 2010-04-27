#include "../ControllerInterface.h"

#ifdef CIFACE_USE_OSX

#include "OSX.h"
#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>

namespace ciface
{
namespace OSX
{


static IOHIDManagerRef HIDManager = NULL;
static CFStringRef OurRunLoop = CFSTR("DolphinOSXInput");

unsigned int KeyboardIndex = 0;


static void DeviceMatching_callback(void* inContext,
									IOReturn inResult,
									void* inSender,
									IOHIDDeviceRef inIOHIDDeviceRef)
{
	NSLog(@"-------------------------");
	NSLog(@"Got Device: %@", IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDProductKey)));
	
	// Add to the devices vector if it's of a type we want
	if (IOHIDDeviceConformsTo(inIOHIDDeviceRef, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) ||
		IOHIDDeviceConformsTo(inIOHIDDeviceRef, kHIDPage_GenericDesktop, kHIDUsage_GD_Keypad))
	{
		std::vector<ControllerInterface::Device*> *devices = (std::vector<ControllerInterface::Device*> *)inContext;
		devices->push_back(new Keyboard(inIOHIDDeviceRef, KeyboardIndex++));
	}
	/*
	// One of the built-in mice devices insta-crash :(
	else if (IOHIDDeviceConformsTo(inIOHIDDeviceRef, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse))
	{
	}
	// Probably just a lot of fiddling...but then we can kill SDL dependency (?)
	else if (IOHIDDeviceConformsTo(inIOHIDDeviceRef, kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad))
	{
	}
	*/
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


}
}

#endif
