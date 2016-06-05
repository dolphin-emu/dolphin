//
//  CCIApplicationCubeControllerView.m
//  CloudConsoleiOS
//
//  Created by Will Cobb on 1/18/16.
//  Copyright © 2016 Will Cobb. All rights reserved.
//

#import "GCControllerView.h"
#import "WCDirectionalControl.h"
//#include "InputCommon/GCPadStatus.h"

enum PadButton
{
    PAD_BUTTON_LEFT  = 0x0001,
    PAD_BUTTON_RIGHT = 0x0002,
    PAD_BUTTON_DOWN  = 0x0004,
    PAD_BUTTON_UP    = 0x0008,
    PAD_TRIGGER_Z    = 0x0010,
    PAD_TRIGGER_R    = 0x0020,
    PAD_TRIGGER_L    = 0x0040,
    PAD_BUTTON_A     = 0x0100,
    PAD_BUTTON_B     = 0x0200,
    PAD_BUTTON_X     = 0x0400,
    PAD_BUTTON_Y     = 0x0800,
    PAD_BUTTON_START = 0x1000,
};

@implementation GCControllerView

- (id)initWithFrame:(CGRect)frame
{
    if (self = [super initWithFrame:frame]) {
        self.multipleTouchEnabled = YES;
        
        buttons = [NSMutableArray new];
        /*  Set up Buttons  */
        //A
        [self addButtonWithFrame:CGRectMake(frame.size.width - 135,
                                            frame.size.height - 215,
                                            80,
                                            80)
                             Tag:PAD_BUTTON_A
                           Image:@"gcpad_a.png"
                    PressedImage:@"gcpad_a_pressed.png"];
        //B
        [self addButtonWithFrame:CGRectMake(frame.size.width - 195,
                                            frame.size.height - 200,
                                            50,
                                            50)
                             Tag:PAD_BUTTON_B
                           Image:@"gcpad_b.png"
                    PressedImage:@"gcpad_b_pressed.png"];
        
        //X
        [self addButtonWithFrame:CGRectMake(frame.size.width - 60,
                                            frame.size.height - 205,
                                            50,
                                            50)
                             Tag:PAD_BUTTON_X
                           Image:@"gcpad_x.png"
                    PressedImage:@"gcpad_x_pressed.png"];
        //Y
        [self addButtonWithFrame:CGRectMake(frame.size.width - 130,
                                            frame.size.height - 260,
                                            50,
                                            50)
                             Tag:PAD_BUTTON_Y
                           Image:@"gcpad_y.png"
                    PressedImage:@"gcpad_y_pressed.png"];
        
        //Z
        [self addButtonWithFrame:CGRectMake(frame.size.width - 120,
                                            frame.size.height - 125,
                                            50,
                                            50)
                             Tag:PAD_TRIGGER_Z
                           Image:@"gcpad_z.png"
                    PressedImage:@"gcpad_z_pressed.png"];
        //R
        [self addButtonWithFrame:CGRectMake(frame.size.width - 80,
                                            frame.size.height/2 - 160,
                                            60,
                                            60)
                             Tag:PAD_TRIGGER_R
                           Image:@"gcpad_r.png"
                    PressedImage:@"gcpad_r_pressed.png"];
        
        //L
        [self addButtonWithFrame:CGRectMake(20,
                                            frame.size.height/2 - 160,
                                            60,
                                            60)
                             Tag:PAD_TRIGGER_L
                           Image:@"gcpad_l.png"
                    PressedImage:@"gcpad_l_pressed.png"];
        //Start
        [self addButtonWithFrame:CGRectMake(frame.size.width/2 - 20,
                                            frame.size.height  - 45,
                                            40,
                                            40)
                             Tag:PAD_BUTTON_START
                           Image:@"gcpad_start.png"
                    PressedImage:@"gcpad_start_pressed.png"];
        
        //Dpad
        NSArray *dpadImages = @[[UIImage imageNamed:@"gcpad_dpad.png"],
                                [UIImage imageNamed:@"gcpad_dpad_pressed_up.png"],
                                [UIImage imageNamed:@"gcpad_dpad_pressed_upright.png"],
                                [UIImage imageNamed:@"gcpad_dpad_pressed_right.png"],
                                [UIImage imageNamed:@"gcpad_dpad_pressed_downright.png"],
                                [UIImage imageNamed:@"gcpad_dpad_pressed_down.png"],
                                [UIImage imageNamed:@"gcpad_dpad_pressed_downleft.png"],
                                [UIImage imageNamed:@"gcpad_dpad_pressed_left.png"],
                                [UIImage imageNamed:@"gcpad_dpad_pressed_upleft.png"]];
        
        WCDirectionalControl *dpad =    [[WCDirectionalControl alloc]
                                         initWithFrame:CGRectMake(130, frame.size.height - 130, 110, 110)
                                         DPadImages:dpadImages];
        dpad.tag = 0;
        [self addSubview:dpad];
        [dpad addTarget:self action:@selector(dpadChanged:) forControlEvents:UIControlEventValueChanged];
        
        //Joystick
        WCDirectionalControl *leftJoy = [[WCDirectionalControl alloc]
                                          initWithFrame:CGRectMake(10,
                                                                   self.frame.size.height - 230,
                                                                   140,
                                                                   140)
                                          BoundsImage:@"gcpad_joystick_range.png"
                                          StickImage:@"gcpad_joystick.png"];
        leftJoy.tag = 0;
        [self addSubview:leftJoy];
        [leftJoy addTarget:self action:@selector(joystickMoved:) forControlEvents:UIControlEventValueChanged];
        //C Stick
        WCDirectionalControl *cJoy =    [[WCDirectionalControl alloc]
                                         initWithFrame:CGRectMake(self.frame.size.width - 240,
                                                                  self.frame.size.height - 130,
                                                                  110,
                                                                  110)
                                         BoundsImage:@"gcpad_joystick_range.png"
                                         StickImage:@"gcpad_joystick.png"];
        cJoy.tag = 1;
        [self addSubview:cJoy];
        [cJoy addTarget:self action:@selector(joystickMoved:) forControlEvents:UIControlEventValueChanged];

        self.alpha = 0.7;
    }
    return self;
}


- (void)addButtonWithFrame:(CGRect)frame Tag:(NSInteger) tag Image:(NSString *) image PressedImage:(NSString *)pressedImage
{
    UIButton *button = [[UIButton alloc] initWithFrame:frame];
    button.tag = tag;
    [button setImage:[UIImage imageNamed:image] forState:UIControlStateNormal];
    if (pressedImage)
        [button setImage:[UIImage imageNamed:pressedImage] forState:UIControlStateHighlighted];
    //[button addTarget:self action:@selector(buttonDown:)   forControlEvents:UIControlEventAllEvents];
    button.userInteractionEnabled = NO;
    [self addSubview:button];
    [buttons addObject:button];
}

- (void)dpadChanged:(WCDirectionalControl *)dpad
{
    _buttonState &=  ~(15 << dpad.tag); //Clear the 4 bits
    _buttonState |= (dpad.direction << dpad.tag); //Set the 4 bits
    if ([self.delegate respondsToSelector:@selector(buttonStateChanged:)]) {
        [self.delegate buttonStateChanged:self.buttonState];
    } else {
        NSLog(@"Error. Delegate doesnt respond to buttonStateChanged:");
    }
}

//for testing
- (NSString *)getBitStringFor16:(uint16_t)value {
    NSString *bits = @"";
    
    for(int i = 0; i < 16; i ++) {
        bits = [NSString stringWithFormat:@"%i%@", value & (1 << i) ? 1 : 0, bits];
    }
    
    return bits;
}

- (void)joystickMoved:(WCDirectionalControl *)joystick
{
    [self.delegate joystick:joystick.tag movedToPosition:(CGPoint)joystick.joyLocation];
}

/*- (void)buttonChanged:(UIButton *)sender
 {
 if (sender.highlighted) {
 NSLog(@"highlighted");
 _buttonState |= sender.tag;
 } else {
 NSLog(@"not highlighted");
 _buttonState &= ~sender.tag;
 }
 if ([self.delegate respondsToSelector:@selector(buttonStateChanged:)]) {
 [self.delegate buttonStateChanged:self.buttonState];
 } else {
 NSLog(@"Error. Delegate doesnt respond to buttonStateChanged:");
 }
 }*/

- (void)trackTouches:(NSSet<UITouch *> *)touches
{
    uint32_t added = 0;
    CGPoint touchPoint;
    for (UIButton *button in buttons) {
        for (UITouch *touch in touches) {
            touchPoint = [touch locationInView:self];
            if (CGRectContainsPoint(button.frame, touchPoint)) {
                _buttonState |= button.tag;
                added |= button.tag;
                button.highlighted = YES;
            } else if (!(button.tag & added)) {
                //NSLog(@"%@ not in %@", NSStringFromCGPoint(touchPoint), NSStringFromCGRect(button.frame));
                _buttonState &= ~button.tag;
                button.highlighted = NO;
            }
        }
    }
    if ([self.delegate respondsToSelector:@selector(buttonStateChanged:)]) {
        [self.delegate buttonStateChanged:self.buttonState];
    } else {
        NSLog(@"Error. Delegate doesnt respond to buttonStateChanged:");
    }
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self trackTouches:touches];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self trackTouches:touches];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UIButton *button in buttons) {
        button.highlighted = NO;
    }
    if ([self.delegate respondsToSelector:@selector(buttonStateChanged:)]) {
        [self.delegate buttonStateChanged:0];
    } else {
        NSLog(@"Error. Delegate doesnt respond to buttonStateChanged:");
    }
}

@end
