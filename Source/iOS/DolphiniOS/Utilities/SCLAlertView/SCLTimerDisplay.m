//
//  SCLTimerDisplay.m
//  SCLAlertView
//
//  Created by Taylor Ryan on 8/18/15.
//  Copyright (c) 2015 AnyKey Entertainment. All rights reserved.
//

#import "SCLTimerDisplay.h"

@interface SCLTimerDisplay ()

@property (strong, nonatomic) UILabel *countLabel;

@end

@implementation SCLTimerDisplay

#define DEGREES_TO_RADIANS(degrees)  ((M_PI * degrees)/ 180)
#define TIMER_STEP .01
#define START_DEGREE_OFFSET -90

@synthesize currentAngle;

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        self.backgroundColor = [UIColor clearColor];
        currentAngle = 0.0f;
    }
    return self;
}

- (instancetype)initWithOrigin:(CGPoint)origin radius:(CGFloat)r
{
    return [self initWithOrigin:(CGPoint)origin radius:r lineWidth:5.0f];
}

- (instancetype)initWithOrigin:(CGPoint)origin radius:(CGFloat)r lineWidth:(CGFloat)width
{
    self = [super initWithFrame:CGRectMake(origin.x, origin.y, r*2, r*2)];
    if (self) {
        self.backgroundColor = [UIColor clearColor];
        currentAngle = START_DEGREE_OFFSET;
        radius = r-(width/2);
        lineWidth = width;
        self.color = [UIColor whiteColor];
        self.userInteractionEnabled = NO;
        
        // Add count label
        _countLabel = [[UILabel alloc] init];
        _countLabel.textColor = [UIColor whiteColor];
        _countLabel.backgroundColor = [UIColor clearColor];
        _countLabel.font = [UIFont fontWithName: @"HelveticaNeue-Bold" size:12.0f];
        _countLabel.textAlignment = NSTextAlignmentCenter;
        _countLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin;
        [self addSubview:_countLabel];
    }
    return self;
}

- (void)updateFrame:(CGSize)size
{
    CGFloat r = radius+(lineWidth/2);
    
    CGFloat originX = size.width - (2*r) - 5;
    CGFloat originY = (size.height - (2*r))/2;
    
    self.frame = CGRectMake(originX, originY, r*2, r*2);
    self.countLabel.frame = CGRectMake(0, 0, r*2, r*2);
}

- (void)drawRect:(CGRect)rect
{
    UIBezierPath* aPath = [UIBezierPath bezierPathWithArcCenter:CGPointMake(radius+(lineWidth/2), radius+(lineWidth/2))
                                                         radius:radius
                                                     startAngle:DEGREES_TO_RADIANS(START_DEGREE_OFFSET)
                                                       endAngle:DEGREES_TO_RADIANS(currentAngle)
                                                      clockwise:YES];
    [self.color setStroke];
    aPath.lineWidth = lineWidth;
    [aPath stroke];
    
    _countLabel.text = [NSString stringWithFormat:@"%d", (int)currentTime];
}

- (void)startTimerWithTimeLimit:(int)tl completed:(SCLActionBlock)completed
{
    if (_reverse)
    {
        currentTime = tl;
    }
    timerLimit = tl;
    timer = [NSTimer scheduledTimerWithTimeInterval:TIMER_STEP target:self selector:@selector(updateTimerButton:) userInfo:nil repeats:YES];
    completedBlock = completed;
    _countLabel.textColor = _color;
}

- (void)cancelTimer
{
    [timer invalidate];
}

- (void)stopTimer
{
    [timer invalidate];
    if (completedBlock != nil) {
        completedBlock();
    }
}

- (void)updateTimerButton:(NSTimer *)timer
{
    if (_reverse)
    {
        currentTime -= TIMER_STEP;
        currentAngle = (currentTime/timerLimit) * 360 + START_DEGREE_OFFSET;        
        
        if(currentTime <= 0) {
            [self stopTimer];
        }
    }
    else {
        currentTime += TIMER_STEP;
        currentAngle = (currentTime/timerLimit) * 360 + START_DEGREE_OFFSET;

        if(currentAngle >= (360 + START_DEGREE_OFFSET)) {
            [self stopTimer];
        }
    }
    [self setNeedsDisplay];
}

@end
