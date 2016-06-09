//
//  ButtonControl.h
//  CC
//
//  Created by CC on 7/5/13.
//  Copyright (c) 2014 CC. All rights reserved.
//

#import "WCDirectionalControl.h"

// This class really doesn't do much. It's basically here to make the code easier to read, but also in case of future expansion.

// Below are identical to the superclass variants, just renamed for clarity
typedef NS_ENUM(NSInteger, WCButtonControlButton) {
    WCButtonControlButtonX     = 1 << 0,
    WCButtonControlButtonB     = 1 << 1,
    WCButtonControlButtonY     = 1 << 2,
    WCButtonControlButtonA     = 1 << 3,
};

@interface WCButtonControl : WCDirectionalControl

@property (readonly, nonatomic) WCButtonControlButton selectedButtons;

@end
