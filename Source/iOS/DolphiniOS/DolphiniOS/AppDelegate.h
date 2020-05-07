// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow* window;
@property (nonatomic) UIBackgroundTaskIdentifier m_save_state_task;

+ (void)Shutdown;

@end
