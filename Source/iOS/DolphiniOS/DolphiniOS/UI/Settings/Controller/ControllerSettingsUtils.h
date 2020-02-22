// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <Foundation/Foundation.h>

#import <optional>

#import "Core/HW/SI/SI.h"
#import "Core/HW/SI/SI_Device.h"
#import "Core/HW/Wiimote.h"

#import "InputCommon/ControllerEmu/ControllerEmu.h"

NS_ASSUME_NONNULL_BEGIN

@interface ControllerSettingsUtils : NSObject

+ (std::optional<int>)SIDevicesToGCMenuIndex:(SerialInterface::SIDevices)si_device;
+ (std::optional<SerialInterface::SIDevices>)SIDevicesFromGCMenuIndex:(int)idx;
+ (size_t)GetTotalAvailableSIDevices;
+ (NSString*)GetLocalizedGameCubeControllerFromIndex:(NSInteger)idx;
+ (size_t)GetTotalAvailableWiimoteTypes;
+ (NSString*)GetLocalizedWiimoteStringFromSource:(WiimoteSource)source;
+ (NSString*)RemoveAndroidFromDeviceName:(NSString*)name;
+ (NSString*)FormatExpression:(NSString*)expression;
+ (bool)IsControllerConnectedToTouchscreen:(ControllerEmu::EmulatedController*)controller;
+ (bool)ShouldControllerSupportFullMotion:(ControllerEmu::EmulatedController*)controller;
+ (void)LoadDefaultProfileOnController:(ControllerEmu::EmulatedController*)controller is_wii:(bool)is_wii type:(NSString*)type;
+ (void)LoadProfileOnController:(ControllerEmu::EmulatedController*)controller device:(std::string)device is_wii:(bool)is_wii profile_path:(std::string)profile_path;
+ (void)SaveSettings:(bool)is_wii;

@end

NS_ASSUME_NONNULL_END
