// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QuartzCore/QuartzCore.h>
#include <UIKit/UIKit.h>

@interface ViewController : UIViewController

- (instancetype)initWithURL:(NSURL*)URL;
@property(nonatomic, copy, readonly) NSURL* URL;
@property(nonatomic, strong, readonly) CAEAGLLayer* EAGLLayer;

@end
