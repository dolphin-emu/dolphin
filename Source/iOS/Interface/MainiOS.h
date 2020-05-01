// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#ifdef __cplusplus
#import "Core/Boot/Boot.h"
#endif

void UpdateWiiPointer();

@interface MainiOS : NSObject

+ (void)applicationStart;
+ (NSString*)getUserFolder;
+ (void)importFiles:(NSSet<NSURL*>*)files;
#ifdef __cplusplus
+ (void)startEmulationWithBootParameters:(std::unique_ptr<BootParameters>)params viewController:(UIViewController*)viewController view:(UIView*)view;
#endif
+ (void)stopEmulation;
+ (UIViewController*)getEmulationViewController;
+ (void)gamepadEventOnPad:(int)pad button:(int)button action:(int)action;
+ (void)gamepadMoveEventOnPad:(int)pad axis:(int)axis value:(CGFloat)value;

@end
