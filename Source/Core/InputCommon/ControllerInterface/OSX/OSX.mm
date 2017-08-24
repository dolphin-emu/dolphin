// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <thread>

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/OSX/OSX.h"
#include "InputCommon/ControllerInterface/OSX/OSXJoystick.h"
#include "InputCommon/ControllerInterface/OSX/RunLoopStopper.h"

namespace ciface
{
namespace OSX
{
constexpr CFTimeInterval FOREVER = 1e20;
static std::thread s_hotplug_thread;
static RunLoopStopper s_stopper;
static IOHIDManagerRef HIDManager = nullptr;
static CFStringRef OurRunLoop = CFSTR("DolphinOSXInput");

void DeviceElementDebugPrint(const void* value, void* context)
{
  IOHIDElementRef e = (IOHIDElementRef)value;
  bool recurse = false;
  if (context)
    recurse = *(bool*)context;

  std::string type = "";
  switch (IOHIDElementGetType(e))
  {
  case kIOHIDElementTypeInput_Axis:
    type = "axis";
    break;
  case kIOHIDElementTypeInput_Button:
    type = "button";
    break;
  case kIOHIDElementTypeInput_Misc:
    type = "misc";
    break;
  case kIOHIDElementTypeInput_ScanCodes:
    type = "scancodes";
    break;
  case kIOHIDElementTypeOutput:
    type = "output";
    break;
  case kIOHIDElementTypeFeature:
    type = "feature";
    break;
  case kIOHIDElementTypeCollection:
    type = "collection";
    break;
  }

  std::string c_type = "";
  if (type == "collection")
  {
    switch (IOHIDElementGetCollectionType(e))
    {
    case kIOHIDElementCollectionTypePhysical:
      c_type = "physical";
      break;
    case kIOHIDElementCollectionTypeApplication:
      c_type = "application";
      break;
    case kIOHIDElementCollectionTypeLogical:
      c_type = "logical";
      break;
    case kIOHIDElementCollectionTypeReport:
      c_type = "report";
      break;
    case kIOHIDElementCollectionTypeNamedArray:
      c_type = "namedArray";
      break;
    case kIOHIDElementCollectionTypeUsageSwitch:
      c_type = "usageSwitch";
      break;
    case kIOHIDElementCollectionTypeUsageModifier:
      c_type = "usageModifier";
      break;
    }
  }

  c_type.append(" ");
  NSLog(@"%s%s%spage: 0x%x usage: 0x%x name: %@ "
         "lmin: %ld lmax: %ld pmin: %ld pmax: %ld",
        type.c_str(), type == "collection" ? ":" : "", type == "collection" ? c_type.c_str() : " ",
        IOHIDElementGetUsagePage(e), IOHIDElementGetUsage(e),
        IOHIDElementGetName(e),  // usually just nullptr
        IOHIDElementGetLogicalMin(e), IOHIDElementGetLogicalMax(e), IOHIDElementGetPhysicalMin(e),
        IOHIDElementGetPhysicalMax(e));

  if ((type == "collection") && recurse)
  {
    CFArrayRef elements = IOHIDElementGetChildren(e);
    CFRange range = {0, CFArrayGetCount(elements)};
    // this leaks...but it's just debug code, right? :D
    CFArrayApplyFunction(elements, range, DeviceElementDebugPrint, nullptr);
  }
}

static void DeviceDebugPrint(IOHIDDeviceRef device)
{
#if 0
#define shortlog(x) NSLog(@"%s: %@", x, IOHIDDeviceGetProperty(device, CFSTR(x)));
	NSLog(@"-------------------------");
	NSLog(@"Got Device: %@",
		IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey)));
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
#endif
}

static void* g_window;

static std::string GetDeviceRefName(IOHIDDeviceRef inIOHIDDeviceRef)
{
  const NSString* name = reinterpret_cast<const NSString*>(
      IOHIDDeviceGetProperty(inIOHIDDeviceRef, CFSTR(kIOHIDProductKey)));
  return (name != nullptr) ? StripSpaces([name UTF8String]) : "Unknown device";
}

static void DeviceRemovalCallback(void* inContext, IOReturn inResult, void* inSender,
                                  IOHIDDeviceRef inIOHIDDeviceRef)
{
  g_controller_interface.RemoveDevice([&inIOHIDDeviceRef](const auto* device) {
    const Joystick* joystick = dynamic_cast<const Joystick*>(device);
    if (joystick && joystick->IsSameDevice(inIOHIDDeviceRef))
      return true;

    return false;
  });
  g_controller_interface.InvokeHotplugCallbacks();
  NOTICE_LOG(SERIALINTERFACE, "Removed device: %s", GetDeviceRefName(inIOHIDDeviceRef).c_str());
}

static void DeviceMatchingCallback(void* inContext, IOReturn inResult, void* inSender,
                                   IOHIDDeviceRef inIOHIDDeviceRef)
{
  DeviceDebugPrint(inIOHIDDeviceRef);
  std::string name = GetDeviceRefName(inIOHIDDeviceRef);

  // Add a device if it's of a type we want
  if (IOHIDDeviceConformsTo(inIOHIDDeviceRef, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick) ||
      IOHIDDeviceConformsTo(inIOHIDDeviceRef, kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad) ||
      IOHIDDeviceConformsTo(inIOHIDDeviceRef, kHIDPage_GenericDesktop,
                            kHIDUsage_GD_MultiAxisController))
  {
    g_controller_interface.AddDevice(std::make_shared<Joystick>(inIOHIDDeviceRef, name));
  }

  NOTICE_LOG(SERIALINTERFACE, "Added device: %s", name.c_str());
  g_controller_interface.InvokeHotplugCallbacks();
}

void Init(void* window)
{
  g_window = window;

  HIDManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
  if (!HIDManager)
    ERROR_LOG(SERIALINTERFACE, "Failed to create HID Manager reference");

  IOHIDManagerSetDeviceMatching(HIDManager, nullptr);
  if (IOHIDManagerOpen(HIDManager, kIOHIDOptionsTypeNone) != kIOReturnSuccess)
    ERROR_LOG(SERIALINTERFACE, "Failed to open HID Manager");

  // Callbacks for acquisition or loss of a matching device
  IOHIDManagerRegisterDeviceMatchingCallback(HIDManager, DeviceMatchingCallback, nullptr);
  IOHIDManagerRegisterDeviceRemovalCallback(HIDManager, DeviceRemovalCallback, nullptr);

  // Match devices that are plugged in right now
  IOHIDManagerScheduleWithRunLoop(HIDManager, CFRunLoopGetCurrent(), OurRunLoop);
  while (CFRunLoopRunInMode(OurRunLoop, 0, TRUE) == kCFRunLoopRunHandledSource)
  {
  };
  IOHIDManagerUnscheduleFromRunLoop(HIDManager, CFRunLoopGetCurrent(), OurRunLoop);

  // Enable hotplugging
  s_hotplug_thread = std::thread([] {
    Common::SetCurrentThreadName("IOHIDManager Hotplug Thread");
    NOTICE_LOG(SERIALINTERFACE, "IOHIDManager hotplug thread started");

    IOHIDManagerScheduleWithRunLoop(HIDManager, CFRunLoopGetCurrent(), OurRunLoop);
    s_stopper.AddToRunLoop(CFRunLoopGetCurrent(), OurRunLoop);
    CFRunLoopRunInMode(OurRunLoop, FOREVER, FALSE);
    s_stopper.RemoveFromRunLoop(CFRunLoopGetCurrent(), OurRunLoop);
    IOHIDManagerUnscheduleFromRunLoop(HIDManager, CFRunLoopGetCurrent(), OurRunLoop);

    NOTICE_LOG(SERIALINTERFACE, "IOHIDManager hotplug thread stopped");
  });
}

void PopulateDevices(void* window)
{
  DeInit();
  Init(window);
}

void DeInit()
{
  s_stopper.Signal();
  s_hotplug_thread.join();

  // This closes all devices as well
  IOHIDManagerClose(HIDManager, kIOHIDOptionsTypeNone);
  CFRelease(HIDManager);
}
}
}
