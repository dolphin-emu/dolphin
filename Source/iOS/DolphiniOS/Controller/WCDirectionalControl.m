//
//  WCDirectionalControl.m
//  CC
//
//  Created by CC
//  Copyright (c) 2014 CC. All rights reserved.
//

#import "WCDirectionalControl.h"

@interface WCDirectionalControl() {
    CALayer *buttonImage;
    CFTimeInterval lastMove;
    
    NSArray *dpadImages;
}

@property (readwrite, nonatomic) WCDirectionalControlDirection direction;
@property (assign, nonatomic) CGSize deadZone; // dead zone in the middle of the control
@property (strong, nonatomic) UIImageView *backgroundImageView;
@property (assign, nonatomic) CGRect deadZoneRect;

@end

@implementation WCDirectionalControl

- (id)initWithFrame:(CGRect)frame BoundsImage:(NSString *)boundsImage StickImage:(NSString *)stickImage
{
    if (self = [super initWithFrame:frame]) {
        // Initialization code
        _backgroundImageView = [[UIImageView alloc] initWithFrame:self.bounds];
        _backgroundImageView.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
        _backgroundImageView.image = [UIImage imageNamed:boundsImage];
        [self addSubview:_backgroundImageView];
        
        buttonImage = [[CALayer alloc] init];
        buttonImage.frame = CGRectMake(0, 0, self.frame.size.width/2.2, self.frame.size.width/2.2);
        buttonImage.contents = (id)[UIImage imageNamed:stickImage].CGImage;
        buttonImage.anchorPoint = CGPointMake(0.5, 0.5);
        buttonImage.actions = @{@"position": [NSNull null]};
        [self.layer addSublayer:buttonImage];
        
        self.deadZone = CGSizeMake(MIN(self.frame.size.width/7, 20), MIN(self.frame.size.height/7, 20));
        
        

        [self setStyle:WCDirectionalControlStyleJoystick];
    }
    return self;
}

//Accepts 1, 5, or 9 images
- (id)initWithFrame:(CGRect)frame DPadImages:(NSArray <UIImage *> *)images;
{
    if (self = [super initWithFrame:frame]) {
        dpadImages = images;
        // Initialization code
        _backgroundImageView = [[UIImageView alloc] initWithFrame:self.bounds];
        _backgroundImageView.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
        _backgroundImageView.image = dpadImages[0];
        [self addSubview:_backgroundImageView];
        
        
        self.deadZone = CGSizeMake(self.frame.size.width/3.1, self.frame.size.height/3.1);
        self.direction = 0;
        
        [self setStyle:WCDirectionalControlStyleDPad];
    }
    return self;
}

- (void) frameUpdated
{
    self.deadZone = CGSizeMake(self.frame.size.width/3, self.frame.size.height/3);
    self.deadZoneRect = CGRectMake((self.bounds.size.width - self.deadZone.width)/2, (self.bounds.size.height - self.deadZone.height)/2, self.deadZone.width, self.deadZone.height);
    buttonImage.frame = CGRectMake(0, 0, self.frame.size.width/2.2, self.frame.size.width/2.2);
    buttonImage.position = self.backgroundImageView.center;
}

- (CGPoint)joyLocation
{
    if (self.style == WCDirectionalControlStyleDPad) {
        NSLog(@"Warning: requesting joystick location for dpad.");
        return CGPointZero;
    }
    CGPoint loc = buttonImage.position;
    loc.x -= self.bounds.size.width /2;
    loc.y -= self.bounds.size.height/2;
    loc.y = -loc.y;
    loc.x /= (self.frame.size.width * 0.45);
    loc.y /= (self.frame.size.height* 0.45);
    return loc;
}

- (void) layoutSubviews
{
    [super layoutSubviews];
    [self frameUpdated];
}


- (WCDirectionalControlDirection)directionForTouch:(UITouch *)touch
{
    if (!touch) {
        if (dpadImages.count == 9) {
            self.backgroundImageView.image = dpadImages[0];
        }
        return 0;
    }
    // convert coords to based on center of control
    CGPoint loc = [touch locationInView:self];
    //if (!CGRectContainsPoint(self.bounds, loc)) return 0;
    WCDirectionalControlDirection direction = 0;
    
    if (loc.x > (self.bounds.size.width + self.deadZone.width)/2) direction |= WCDirectionalControlDirectionRight;
    else if (loc.x < (self.bounds.size.width - self.deadZone.width)/2) direction |= WCDirectionalControlDirectionLeft;
    if (loc.y > (self.bounds.size.height + self.deadZone.height)/2) direction |= WCDirectionalControlDirectionDown;
    else if (loc.y < (self.bounds.size.height - self.deadZone.height)/2) direction |= WCDirectionalControlDirectionUp;
    
    if (dpadImages.count == 9) {
        if (direction & WCDirectionalControlDirectionUp) {
            if (direction & WCDirectionalControlDirectionRight) {
                self.backgroundImageView.image = dpadImages[2]; //upleft
            } else if (direction & WCDirectionalControlDirectionLeft) {
                self.backgroundImageView.image = dpadImages[8]; //upright
            } else {
                self.backgroundImageView.image = dpadImages[1]; //up
            }
        } else if (direction & WCDirectionalControlDirectionDown) {
            if (direction & WCDirectionalControlDirectionRight) {
                self.backgroundImageView.image = dpadImages[4]; //downleft
            } else if (direction & WCDirectionalControlDirectionLeft) {
                self.backgroundImageView.image = dpadImages[6]; //downright
            } else {
                self.backgroundImageView.image = dpadImages[5]; //down
            }
        } else if (direction & WCDirectionalControlDirectionRight) {
            self.backgroundImageView.image = dpadImages[3]; //right
        } else if (direction & WCDirectionalControlDirectionLeft) {
            self.backgroundImageView.image = dpadImages[7]; //left
        } else {
            self.backgroundImageView.image = dpadImages[0]; //None
        }
    }
    
    return direction;
}


- (BOOL)beginTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event
{
    if (self.style == WCDirectionalControlStyleJoystick) {
        CGPoint loc = [touch locationInView:self];
        buttonImage.position = loc;
        lastMove = CACurrentMediaTime();
    } else {
        self.direction = [self directionForTouch:touch];
    }
    
    [self sendActionsForControlEvents:UIControlEventValueChanged];
    
    return YES;
}

- (BOOL)continueTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event
{
    if (self.style == WCDirectionalControlStyleJoystick) {
        // keep button inside
        CGPoint loc = [touch locationInView:self];
        loc.x -= self.bounds.size.width/2;
        loc.y -= self.bounds.size.height/2;
        double radius = sqrt(loc.x*loc.x+loc.y*loc.y);
        double maxRadius = self.bounds.size.width * 0.45;
        if (radius > maxRadius) {
            double angle = atan(loc.y/loc.x);
            if (loc.x < 0) angle += M_PI;
            loc.x = maxRadius * cos(angle);
            loc.y = maxRadius * sin(angle);
        }
        loc.x += self.bounds.size.width/2;
        loc.y += self.bounds.size.height/2;
        if (CACurrentMediaTime() - lastMove > 0.030) { // increasing this value reduces refresh rate and greatly increases performance
            buttonImage.position = loc;
            lastMove = CACurrentMediaTime();
        }
    } else {
        self.direction = [self directionForTouch:touch];
    }
    
    [self sendActionsForControlEvents:UIControlEventValueChanged];
    return YES;
}

- (void)endTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event
{
    if (self.style == WCDirectionalControlStyleJoystick) {
        buttonImage.position = CGPointMake(self.bounds.size.width/2, self.bounds.size.height/2);
    } else {
        self.direction = [self directionForTouch:nil];
    }
    
    [self sendActionsForControlEvents:UIControlEventValueChanged];
}


@end
