//
//  ButtonControl.h
//  Dolphin
//
//  Created by Will Cobb on 5/31/16.
//

#import "Controller/WCDirectionalControl.h"

typedef NS_ENUM(NSInteger, WCButtonControlButton)
{
	WCButtonControlButtonX	 = 1 << 0,
	WCButtonControlButtonB	 = 1 << 1,
	WCButtonControlButtonY	 = 1 << 2,
	WCButtonControlButtonA	 = 1 << 3,
};

@interface WCButtonControl : WCDirectionalControl

@property (readonly, nonatomic) WCButtonControlButton selectedButtons;

@end
