//
//  ButtonControl.m
//  CC
//
//  Created by Riley Testut on 7/5/13.
//  Copyright (c) 2014 CC. All rights reserved.
//

#import "WCButtonControl.h"

@interface WCDirectionalControl ()

@property (strong, nonatomic) UIImageView *backgroundImageView;

@end

@interface WCButtonControl ()

@property (readwrite, nonatomic) WCButtonControlButton selectedButtons;

@end

@implementation WCButtonControl

- (id)initWithCoder:(NSCoder *)aDecoder
{
    self = [super initWithCoder:aDecoder];
    if (self) {
        // Initialization code
        self.backgroundImageView.image = [UIImage imageNamed:@"ABXYPad"];
    }
    return self;
}

- (WCButtonControlButton)selectedButtons {
    return (WCButtonControlButton)self.direction;
}

@end
