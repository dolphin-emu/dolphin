// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerSettingsUtils.h"

#import <map>

#import "Core/HW/GCPad.h"
#import "Core/HW/Wiimote.h"

#import "InputCommon/ControllerInterface/iOS/iOS.h"
#import "InputCommon/InputConfig.h"

@implementation ControllerSettingsUtils

static const std::map<SerialInterface::SIDevices, int> s_gc_types = {
    {SerialInterface::SIDEVICE_NONE, 0},         {SerialInterface::SIDEVICE_GC_CONTROLLER, 1},
    {SerialInterface::SIDEVICE_WIIU_ADAPTER, 2}, {SerialInterface::SIDEVICE_GC_STEERING, 3},
    {SerialInterface::SIDEVICE_DANCEMAT, 4},     {SerialInterface::SIDEVICE_GC_TARUKONGA, 5},
    {SerialInterface::SIDEVICE_GC_GBA, 6},       {SerialInterface::SIDEVICE_GC_KEYBOARD, 7}};

+ (std::optional<int>)SIDevicesToGCMenuIndex:(SerialInterface::SIDevices)si_device
{
  auto it = s_gc_types.find(si_device);
  return it != s_gc_types.end() ? it->second : std::optional<int>();
}

+ (std::optional<SerialInterface::SIDevices>)SIDevicesFromGCMenuIndex:(int)idx
{
  auto it = std::find_if(s_gc_types.begin(), s_gc_types.end(),
                         [=](auto pair) { return pair.second == idx; });
  return it != s_gc_types.end() ? it->first : std::optional<SerialInterface::SIDevices>();
}

+ (size_t)GetTotalAvailableSIDevices
{
  return s_gc_types.size();
}

+ (NSString*)GetLocalizedGameCubeControllerFromIndex:(NSInteger)idx
{
  switch (idx)
  {
    case 0:
      return @"None";
    case 1:
      return @"Standard Controller";
    case 2:
      return @"GameCube Adapter for Wii U";
    case 3:
      return @"Steering Wheel";
    case 4:
      return @"Dance Mat";
    case 5:
      return @"DK Bongos";
    case 6:
      return @"GBA";
    case 7:
      return @"Keyboard";
    default:
      return @"Unknown";
  }
}

+ (size_t)GetTotalAvailableWiimoteTypes
{
  return 3;
}

+ (NSString*)GetLocalizedWiimoteStringFromIndex:(NSInteger)idx
{
  switch (idx)
  {
    case 0:
      return @"None";
    case 1:
      return @"Emulated";
    case 2:
      return @"Real";
    default:
      return @"Unknown";
  }
}

// "Android" is still the source for the Touchscreen device for compatibility reasons.
// We hide this to avoid the user getting confused.
+ (NSString*)RemoveAndroidFromDeviceName:(NSString*)name
{
  return [name stringByReplacingOccurrencesOfString:@"Android" withString:@"iOS"];
}

+ (NSString*)FormatExpression:(NSString*)expression
{
  return [expression stringByReplacingOccurrencesOfString:@"`" withString:@""];
}

+ (bool)DoesDeviceSupportFullMotion:(ciface::Core::Device*)device
{
  std::string device_source = device->GetSource();
  if (device_source == "Android" || device_source == "DSUClient")
  {
    return true;
  }
  
  ciface::iOS::Controller* controller = dynamic_cast<ciface::iOS::Controller*>(device);
  if (controller)
  {
    return controller->SupportsAccelerometer() && controller->SupportsGyroscope();
  }
  
  return false;
}

+ (void)SaveSettings:(bool)is_wii
{
  if (is_wii)
  {
    Wiimote::GetConfig()->SaveConfig();
  }
  else
  {
    Pad::GetConfig()->SaveConfig();
  }
}

@end
