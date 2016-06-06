//
//  ButtonControl.h
//  Dolphin
//
//  Created by Will Cobb on 5/31/16.
//

#import "Controller/WCDirectionalControl.h"

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
