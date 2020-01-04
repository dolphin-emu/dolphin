// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface MainiOS : NSObject

+ (void)applicationStart;
+ (NSString*)getUserFolder;
+ (NSString*)getGfxBackend;
+ (void)importFiles:(NSSet<NSURL*>*)files;
+ (void)startEmulationWithFile:(NSString*)file viewController:(UIViewController*)viewController view:(UIView*)view;
+ (void)stopEmulation;
+ (CGFloat)getGameAspectRatio;
+ (CGRect)getRenderTargetRectangle;
+ (void)setDrawRectangleCustomOriginAsX:(int)x y:(int)y;
+ (void)windowResized;
+ (void)gamepadEventOnPad:(int)pad button:(int)button action:(int)action;
+ (void)gamepadMoveEventOnPad:(int)pad axis:(int)axis value:(CGFloat)value;
+ (void)toggleImuIr:(bool)enabled;
+ (void)toggleSidewaysWiimote:(bool)enabled reload_wiimote:(bool)reload_wiimote;
+ (bool)isTouchscreenDeviceConnected:(bool)check_wii;

@end
