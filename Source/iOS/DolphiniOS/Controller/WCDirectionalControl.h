//
//  WCDirectionalControl.h
//  CC
//
//  Created by CC
//  Copyright (c) 2014 CC. All rights reserved.
//

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, WCDirectionalControlDirection) {
    WCDirectionalControlDirectionLeft   = 1 << 0,
    WCDirectionalControlDirectionRight  = 1 << 1,
    WCDirectionalControlDirectionDown   = 1 << 2,
    WCDirectionalControlDirectionUp     = 1 << 3,
};

typedef NS_ENUM(NSInteger, WCDirectionalControlStyle) {
    WCDirectionalControlStyleDPad = 0,
    WCDirectionalControlStyleJoystick = 1,
};

@interface WCDirectionalControl : UIControl

@property (readonly, nonatomic) WCDirectionalControlDirection direction;
@property (assign, nonatomic) WCDirectionalControlStyle style;

- (id)initWithFrame:(CGRect)frame BoundsImage:(NSString *)boundsImage StickImage:(NSString *)stickImage;
- (id)initWithFrame:(CGRect)frame DPadImages:(NSArray <UIImage *> *)dpadImages;

- (CGPoint)joyLocation;
- (void) frameUpdated;

@end
