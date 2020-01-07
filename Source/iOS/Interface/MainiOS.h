// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

void UpdateWiiPointer();

@interface MainiOS : NSObject

+ (void)applicationStart;
+ (NSString*)getUserFolder;
+ (void)importFiles:(NSSet<NSURL*>*)files;
+ (void)startEmulationWithFile:(NSString*)file viewController:(UIViewController*)viewController view:(UIView*)view;
+ (void)stopEmulation;
+ (UIViewController*)getEmulationViewController;
+ (CGFloat)getGameAspectRatio;
+ (CGRect)getRenderTargetRectangle;
+ (void)windowResized;
+ (void)gamepadEventOnPad:(int)pad button:(int)button action:(int)action;
+ (void)gamepadMoveEventOnPad:(int)pad axis:(int)axis value:(CGFloat)value;

@end
