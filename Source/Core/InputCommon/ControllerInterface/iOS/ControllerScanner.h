// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <Foundation/Foundation.h>

@interface ControllerScanner : NSObject

- (id)init;
- (void)BeginControllerScan;
- (void)ControllerConnected:(NSNotification*)notification;
- (void)ControllerDisconnected:(NSNotification*)notification;

@end
