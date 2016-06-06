//
//  ButtonControl.m
//  Dolphin
//
//  Created by Will Cobb on 5/31/16.
//

#import "Controller/WCButtonControl.h"

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
