// Copyright 2016 WillCobb
// Licensed under GPLV2+
// Refer to the license.txt provided

#import "WCDirectionalControl.h"

@protocol GCControllerViewDelegate <NSObject>

- (void)buttonStateChanged:(uint16_t)buttonState;
- (void)joystick:(NSInteger)joyid movedToPosition:(CGPoint)joyPosition;
@end

@interface GCControllerView : UIView
{
	NSMutableArray* buttons;
}

@property uint16_t  buttonState;
@property (weak) id <GCControllerViewDelegate> delegate;

- (void)addButtonWithFrame:(CGRect)frame Tag:(NSInteger) tag Image:(NSString*) image PressedImage:(NSString*)pressedImage;
- (void)joystickMoved:(WCDirectionalControl*)joystick;
- (void)dpadChanged:(WCDirectionalControl*)dpad;

@end
