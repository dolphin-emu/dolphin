// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerSettingsUtils.h"

#import <map>

#import "Core/HW/GCPad.h"
#import "Core/HW/WiimoteEmu/WiimoteEmu.h"

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
      return DOLocalizedString(@"None");
    case 1:
      return DOLocalizedString(@"Standard Controller");
    case 2:
      return DOLocalizedString(@"GameCube Adapter for Wii U");
    case 3:
      return DOLocalizedString(@"Steering Wheel");
    case 4:
      return DOLocalizedString(@"Dance Mat");
    case 5:
      return DOLocalizedString(@"DK Bongos");
    case 6:
      return DOLocalizedString(@"GBA");
    case 7:
      return DOLocalizedString(@"Keyboard");
    default:
      return @"??? (GC)";
  }
}

+ (size_t)GetTotalAvailableWiimoteTypes
{
  return 3;
}

+ (NSString*)GetLocalizedWiimoteStringFromSource:(WiimoteSource)source
{
  switch (source)
  {
    case WiimoteSource::None:
      return DOLocalizedString(@"None");
    case WiimoteSource::Emulated:
      return DOLocalizedString(@"Emulated Wii Remote");
    case WiimoteSource::Real:
      return DOLocalizedString(@"Real Wii Remote");
    default:
      return @"??? (Wii)";
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

+ (bool)IsControllerConnectedToTouchscreen:(ControllerEmu::EmulatedController*)controller
{
  return controller->GetDefaultDevice().source == "Android";
}

+ (bool)ShouldControllerSupportFullMotion:(ControllerEmu::EmulatedController*)controller
{
  ciface::Core::Device* device = g_controller_interface.FindDevice(controller->GetDefaultDevice()).get();
  
  std::string device_source = device->GetSource();
  if (device_source == "DSUClient")
  {
    return true;
  }
  
  ciface::iOS::Controller* ios_controller = dynamic_cast<ciface::iOS::Controller*>(device);
  if (ios_controller)
  {
    return ios_controller->SupportsAccelerometer() && ios_controller->SupportsGyroscope();
  }
  
  return false;
}

+ (void)LoadDefaultProfileOnController:(ControllerEmu::EmulatedController*)controller is_wii:(bool)is_wii type:(NSString*)type
{
  NSString* ini_path;
  if (is_wii)
  {
    ini_path = [NSString stringWithFormat:@"%@WiimoteProfile", type];
  }
  else
  {
    ini_path = [NSString stringWithFormat:@"%@GCPadProfile", type];
  }
  
  NSString* bundle_path = [[NSBundle mainBundle] pathForResource:ini_path ofType:@"ini"];
  std::string path = std::string([bundle_path cStringUsingEncoding:NSUTF8StringEncoding]);
  
  [ControllerSettingsUtils LoadProfileOnController:controller device:controller->GetDefaultDevice().ToString() is_wii:is_wii profile_path:path];
}

+ (void)LoadProfileOnController:(ControllerEmu::EmulatedController*)controller device:(std::string)device is_wii:(bool)is_wii profile_path:(std::string)profile_path
{
  if (device == "")
  {
    device = controller->GetDefaultDevice().ToString();
  }
  
  IniFile ini;
  ini.Load(profile_path);
  controller->LoadConfig(ini.GetOrCreateSection("Profile"));
  
  // Reload the device
  controller->SetDefaultDevice(device);
  
  // Set IMU defaults if motion is supported
  if (is_wii && [ControllerSettingsUtils ShouldControllerSupportFullMotion:controller])
  {
    WiimoteEmu::Wiimote* wiimote = static_cast<WiimoteEmu::Wiimote*>(controller);
    
    ControllerEmu::ControlGroup* imu_accelerometer = wiimote->GetWiimoteGroup(WiimoteEmu::WiimoteGroup::IMUAccelerometer);
    imu_accelerometer->SetControlExpression(0, "Accel Left");
    imu_accelerometer->SetControlExpression(1, "Accel Right");
    imu_accelerometer->SetControlExpression(2, "Accel Forward");
    imu_accelerometer->SetControlExpression(3, "Accel Backward");
    imu_accelerometer->SetControlExpression(4, "Accel Up");
    imu_accelerometer->SetControlExpression(5, "Accel Down");
    
    ControllerEmu::ControlGroup* imu_gyroscope = wiimote->GetWiimoteGroup(WiimoteEmu::WiimoteGroup::IMUGyroscope);
    imu_gyroscope->SetControlExpression(0, "Gyro Pitch Up");
    imu_gyroscope->SetControlExpression(1, "Gyro Pitch Down");
    imu_gyroscope->SetControlExpression(2, "Gyro Roll Left");
    imu_gyroscope->SetControlExpression(3, "Gyro Roll Right");
    imu_gyroscope->SetControlExpression(4, "Gyro Yaw Left");
    imu_gyroscope->SetControlExpression(5, "Gyro Yaw Right");
  }
  
  controller->UpdateReferences(g_controller_interface);
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
