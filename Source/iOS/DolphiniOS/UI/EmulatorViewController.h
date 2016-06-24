// Copyright 2016 WillCobb, OatmealDome
// Licensed under GPLV2+
// Refer to the license.txt provided

#import <UIKit/UIKit.h>

@class GLKView;
@class DolphinGame;
@interface EmulatorViewController : UIViewController

@property IBOutlet GLKView* glkView;

- (void)launchGame:(DolphinGame*)game;

@end

