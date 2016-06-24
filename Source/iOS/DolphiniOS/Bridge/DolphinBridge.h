// Copyright 2016 OatmealDome, WillCobb
// Licensed under GPLV2+
// Refer to the license.txt provided

// Consider making this a singleton

#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>

@protocol DolphinBridgeDelegate <NSObject>

@end

@interface DolphinBridge : NSObject

-(void)openRomAtPath:(NSString*)path inView:(GLKView*)view;

@end