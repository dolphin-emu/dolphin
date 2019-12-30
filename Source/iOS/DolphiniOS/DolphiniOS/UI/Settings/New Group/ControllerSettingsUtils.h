// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <Foundation/Foundation.h>

#import <optional>

#import "Core/HW/SI/SI.h"
#import "Core/HW/SI/SI_Device.h"

#include "InputCommon/ControllerInterface/Device.h"

NS_ASSUME_NONNULL_BEGIN

@interface ControllerSettingsUtils : NSObject

+ (std::optional<int>)SIDevicesToGCMenuIndex:(SerialInterface::SIDevices)si_device;
+ (std::optional<SerialInterface::SIDevices>)SIDevicesFromGCMenuIndex:(int)idx;
+ (size_t)GetTotalAvailableSIDevices;
+ (NSString*)GetLocalizedGameCubeControllerFromIndex:(NSInteger)idx;
+ (size_t)GetTotalAvailableWiimoteTypes;
+ (NSString*)GetLocalizedWiimoteStringFromIndex:(NSInteger)idx;
+ (NSString*)RemoveAndroidFromDeviceName:(NSString*)name;
+ (NSString*)FormatExpression:(NSString*)expression;
+ (bool)DoesDeviceSupportFullMotion:(ciface::Core::Device*)device;
+ (void)SaveSettings:(bool)is_wii;

@end

NS_ASSUME_NONNULL_END
