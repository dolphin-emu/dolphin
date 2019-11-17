// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface MainiOS : NSObject

+ (void) startEmulationWithFile:(NSString*) file view:(UIView*) view;

@end
