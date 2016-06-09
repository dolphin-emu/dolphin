//
//  ZAActivityBar.m
//
//  Created by Zac Altman on 24/11/12.
//  Copyright (c) 2012 Zac Altman. All rights reserved.
//

#import "ZAActivityBar.h"
#import <QuartzCore/QuartzCore.h>
#import "SKBounceAnimation.h"

#define ZA_ANIMATION_SHOW_KEY @"showAnimation"
#define ZA_ANIMATION_DISMISS_KEY @"dismissAnimation"

#ifdef __IPHONE_6_0
#   define UITextAlignmentCenter    NSTextAlignmentCenter
#   define UITextAlignmentLeft      NSTextAlignmentLeft
#   define UITextAlignmentRight     NSTextAlignmentRight
#   define UILineBreakModeTailTruncation     NSLineBreakByTruncatingTail
#   define UILineBreakModeMiddleTruncation   NSLineBreakByTruncatingMiddle
#endif

///////////////////////////////////////////////////////////////

@interface ZAActivityAction : NSObject
@property (nonatomic,strong) NSString *name;
@property (nonatomic,strong) NSString *status;
@end

@implementation ZAActivityAction
@end

///////////////////////////////////////////////////////////////

@interface ZAActivityBar () {
    
    // Why use a dictionary and an array?
    // We need order as well as reference data, so they work together.
    
    NSMutableArray *_actionArray;
    NSMutableDictionary *_actionDict;
}

@property BOOL isVisible;
@property NSUInteger offset;

@property (nonatomic, strong, readonly) UIView *actionIndicatorView;
@property (nonatomic, strong, readonly) UILabel *actionIndicatorLabel;
@property (nonatomic, strong, readonly) NSTimer *fadeOutTimer;
@property (nonatomic, strong, readonly) UIWindow *overlayWindow;
@property (nonatomic, strong, readonly) UIView *barView;
@property (nonatomic, strong, readonly) UILabel *stringLabel;
@property (nonatomic, strong, readonly) UIActivityIndicatorView *spinnerView;
@property (nonatomic, strong, readonly) UIImageView *imageView;

- (void) showWithStatus:(NSString *)status forAction:(NSString *)action;
- (void) setStatus:(NSString*)string;
- (void) showImage:(UIImage*)image status:(NSString*)status duration:(NSTimeInterval)duration forAction:(NSString *)action;

- (void) dismissAll;
- (void) dismissForAction:(NSString *)action;

- (void) registerNotifications;

@end

@implementation ZAActivityBar

@synthesize fadeOutTimer, overlayWindow, barView, stringLabel, spinnerView, imageView, actionIndicatorView, actionIndicatorLabel;

///////////////////////////////////////////////////////////////

#pragma mark - Offset Properties

+ (void) setLocationBottom
{
    [ZAActivityBar sharedView].offset = 0.0f;
}

+ (void) setLocationTabBar
{
    [ZAActivityBar sharedView].offset = 49.0f;
}

+ (void) setLocationNavBar
{
    CGRect screenRect = [[UIScreen mainScreen] bounds];
    [ZAActivityBar sharedView].offset = screenRect.size.height - 120.0f;
}

///////////////////////////////////////////////////////////////

#pragma mark - Action Methods

- (ZAActivityAction *) getPrimaryAction {
    
    if (_actionArray.count == 0)
        return nil;
    
    NSString *action = [_actionArray objectAtIndex:0];
    return [self getAction:action];
    
}

- (ZAActivityAction *) getAction:(NSString *)action {
    
    if (![self actionExists:action])
        return nil;

    return [_actionDict objectForKey:action];
}

- (void) addAction:(NSString *)action withStatus:(NSString *)status {

    ZAActivityAction *a = [self getAction:action];
    
    if (!a) {
        [_actionArray addObject:action];
        a = [ZAActivityAction new];
        a.name = action;
    }

    a.status = status;

    [_actionDict setObject:a forKey:action];
    
    [self updateActionIndicator];
    
}

- (void) removeAction:(NSString *)action {

    if (![self actionExists:action])
        return;

    [_actionDict removeObjectForKey:action];
    [_actionArray removeObject:action];

    [self updateActionIndicator];

}

- (BOOL) isPrimaryAction:(NSString *)action {

    if (![self actionExists:action])
        return NO;
    
    NSUInteger index = [_actionArray indexOfObject:action];
    return index == 0;
    
}

- (BOOL) actionExists:(NSString *)action {
    return [_actionArray containsObject:action];
}

- (void) updateActionIndicator {
    NSUInteger count = [_actionArray count];
    
    // We only need to display this indicator if there is more than one message in the queue
    BOOL makeHidden = (count <= 1);
    BOOL isHidden = (self.actionIndicatorView.alpha == 0.0f);
    
    // Who doesn't love animations
    BOOL shouldAnimate = isHidden != makeHidden;
    
    [self.actionIndicatorLabel setFrame:self.actionIndicatorView.bounds];
    
    // When animating in, we want to change the text before the view is displayed
    // Would be nice to only set the text in a single place...
    if (isHidden) {
        [self setActionIndicatorCount:count];
    }
    
    [UIView animateWithDuration:(shouldAnimate ? 0.1f : 0.0f)
                          delay:0.0f
                        options:UIViewAnimationOptionCurveEaseIn // Not required: |UIViewAnimationOptionBeginFromCurrentState
                     animations:^{
                         self.actionIndicatorView.alpha = (makeHidden ? 0.0f : 1.0f);
                         
                         // When animating out, we don't want the text to change.
                         if (!makeHidden) {
                             [self setActionIndicatorCount:count];
                         }
                     } completion:nil];
    
}

- (void) setActionIndicatorCount:(NSUInteger)count {
    [self.actionIndicatorLabel setText:[NSString stringWithFormat:@"%lu", count]];
    // May want to resize the shape to make a 'pill' shape in the future? Still looks fine for
    // numbers up to 100.
}

///////////////////////////////////////////////////////////////

#pragma mark - Show Methods

+ (void) showWithStatus:(NSString *)status {
    [ZAActivityBar showWithStatus:status forAction:DEFAULT_ACTION];
}

+ (void) showWithStatus:(NSString *)status forAction:(NSString *)action {
    [[ZAActivityBar sharedView] showWithStatus:status forAction:action];
}

+ (void) show {
    [ZAActivityBar showForAction:DEFAULT_ACTION];
}

+ (void) showForAction:(NSString *)action {
    [[ZAActivityBar sharedView] showWithStatus:nil forAction:action];
}

- (void) showWithStatus:(NSString *)status forAction:(NSString *)action {
    dispatch_async(dispatch_get_main_queue(), ^{
        if(!self.superview)
            [self.overlayWindow addSubview:self];
        if ([UIApplication sharedApplication].statusBarHidden) return; //Don't show if in game
        // Add the action
        [self addAction:action withStatus:status];

        // Only continue if the action should be visible.
        BOOL isPrimaryAction = [self isPrimaryAction:action];
        if (!isPrimaryAction)
            return;

        self.fadeOutTimer = nil;
        self.imageView.hidden = YES;

        [self.overlayWindow setHidden:NO];
        [self.spinnerView startAnimating];
        
        [self setStatus:status];
        
        if (!_isVisible) {
            _isVisible = YES;
            
            [self positionBar:nil];
            [self positionOffscreen];
            [self registerNotifications];
            
            // We want to remove the previous animations
            [self removeAnimationForKey:ZA_ANIMATION_DISMISS_KEY];

            NSString *bounceKeypath = @"position.y";
            id bounceOrigin = [NSNumber numberWithFloat:self.barView.layer.position.y];
            id bounceFinalValue = [NSNumber numberWithFloat:[self getBarYPosition]];
            
            SKBounceAnimation *bounceAnimation = [SKBounceAnimation animationWithKeyPath:bounceKeypath];
            [bounceAnimation setValue:ZA_ANIMATION_SHOW_KEY forKey:@"identifier"]; // So we can find identify animations later
            bounceAnimation.fromValue = bounceOrigin;
            bounceAnimation.toValue = bounceFinalValue;
            bounceAnimation.shouldOvershoot = YES;
            bounceAnimation.numberOfBounces = 4;
            bounceAnimation.delegate = self;
            bounceAnimation.removedOnCompletion = YES;
            bounceAnimation.duration = 0.7f;
            
            [self.barView.layer addAnimation:bounceAnimation forKey:@"showAnimation"];
            
            CGPoint position = self.barView.layer.position;
            position.y = [bounceFinalValue floatValue];
            [self.barView.layer setPosition:position];
            
        }
        
    });
}

// Success

+ (void) showSuccessWithStatus:(NSString *)status {
    [ZAActivityBar showSuccessWithStatus:status forAction:DEFAULT_ACTION];
}

+ (void) showSuccessWithStatus:(NSString *)status duration:(NSTimeInterval)duration {
    [ZAActivityBar showSuccessWithStatus:status duration:duration forAction:DEFAULT_ACTION];
}

+ (void) showSuccessWithStatus:(NSString *)status forAction:(NSString *)action {
    [ZAActivityBar showSuccessWithStatus:status duration:1.0f forAction:action];
}

+ (void) showSuccessWithStatus:(NSString *)status duration:(NSTimeInterval)duration forAction:(NSString *)action {
    [ZAActivityBar showImage:[UIImage imageNamed:@"ZAActivityBar.bundle/success.png"]
                      status:status
                    duration:duration
                   forAction:action];
}

// Error

+ (void) showErrorWithStatus:(NSString *)status {
    [ZAActivityBar showErrorWithStatus:status forAction:DEFAULT_ACTION];
}

+ (void) showErrorWithStatus:(NSString *)status duration:(NSTimeInterval)duration {
    [ZAActivityBar showErrorWithStatus:status duration:duration forAction:DEFAULT_ACTION];
}

+ (void) showErrorWithStatus:(NSString *)status forAction:(NSString *)action {
    [ZAActivityBar showErrorWithStatus:status duration:1.0f forAction:action];
}

+ (void) showErrorWithStatus:(NSString *)status duration:(NSTimeInterval)duration forAction:(NSString *)action {
    [ZAActivityBar showImage:[UIImage imageNamed:@"ZAActivityBar.bundle/error.png"]
                      status:status
                    duration:duration
                   forAction:action];
}

// Image

+ (void)showImage:(UIImage *)image status:(NSString *)status {
    [ZAActivityBar showImage:image status:status forAction:DEFAULT_ACTION];
}

+ (void)showImage:(UIImage*)image status:(NSString*)status duration:(NSTimeInterval)duration {
    [ZAActivityBar showImage:image status:status duration:duration forAction:DEFAULT_ACTION];
}

+ (void)showImage:(UIImage *)image status:(NSString *)status forAction:(NSString *)action {
    [ZAActivityBar showImage:image status:status duration:1.0f forAction:action];
}

+ (void)showImage:(UIImage*)image status:(NSString*)status duration:(NSTimeInterval)duration forAction:(NSString *)action {
    [[ZAActivityBar sharedView] showImage:image
                                   status:status
                                 duration:duration
                                forAction:action];
}

- (void)showImage:(UIImage*)image status:(NSString*)status duration:(NSTimeInterval)duration forAction:(NSString *)action {
    
    // Add the action if it doesn't exist yet
    if (![self actionExists:action]) {
        [self addAction:action withStatus:status];
    }

    // Only continue if the action should be visible.
    BOOL isPrimaryAction = [self isPrimaryAction:action];
    if (!isPrimaryAction) {
        [self removeAction:action];
        return;
    }
    
    if(![ZAActivityBar isVisible])
        [ZAActivityBar showForAction:action];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        
        self.imageView.image = image;
        self.imageView.hidden = NO;
        [self setStatus:status];
        [self.spinnerView stopAnimating];
        
        self.fadeOutTimer = [NSTimer scheduledTimerWithTimeInterval:duration
                                                             target:self
                                                           selector:@selector(dismissFromTimer:)
                                                           userInfo:action
                                                            repeats:NO];
    });

}

///////////////////////////////////////////////////////////////

#pragma mark - Property Methods

- (void)setStatus:(NSString *)string {
	
    CGFloat stringWidth = 0;
    CGFloat stringHeight = 0;
    CGRect labelRect = CGRectZero;
    
    if(string) {
        float offset = (SPINNER_SIZE + 2 * ICON_OFFSET);
        float width = self.barView.frame.size.width - offset;
        CGSize stringSize = [string sizeWithFont:self.stringLabel.font
                               constrainedToSize:CGSizeMake(width, 300)];
        stringWidth = stringSize.width;
        stringHeight = stringSize.height;

        labelRect = CGRectMake(offset, 0, stringWidth, HEIGHT);
        
    }
	
	self.stringLabel.hidden = NO;
	self.stringLabel.text = string;
	self.stringLabel.frame = labelRect;
	
}

+ (BOOL)isVisible {
    return [[ZAActivityBar sharedView] isVisible];
}

///////////////////////////////////////////////////////////////

#pragma mark - Dismiss Methods

+ (void) dismiss {
    [[ZAActivityBar sharedView] dismissAll];
}

+ (void) dismissForAction:(NSString *)action {
	[[ZAActivityBar sharedView] dismissForAction:action];
}

- (void) dismissAll {

    for (int i = (_actionArray.count - 1); i >= 0; i--) {
        NSString *action = [_actionArray objectAtIndex:i];
        
        // First item (visible one)
        if (i == 0) {
            [self dismissForAction:action];
        }
        // Non-visible items
        else {
            [self removeAction:action];
        }
    }
}

- (void) dismissForAction:(NSString *)action {
    dispatch_async(dispatch_get_main_queue(), ^{
        
        // Check if the action is on the screen
        BOOL onScreen = [self isPrimaryAction:action];
        
        // Remove the action
        [self removeAction:action];

        // If this is the currently visible action, then let's do some magic
        if (onScreen) {
            
            // Let's get the new primary action
            ZAActivityAction *primaryAction = [self getPrimaryAction];
            
            // And just update the visual.
            if (primaryAction) {
                [ZAActivityBar showWithStatus:primaryAction.status forAction:primaryAction.name];
                return;
            }
            
        }
        // If it's not visible, don't continue.
        else {
            return;
        }

        if (_isVisible) {
            _isVisible = NO;
            
            [[NSNotificationCenter defaultCenter] removeObserver:self];
            
            // If the animation is midway through, we want it to drop immediately
            BOOL shouldDrop = [self.barView.layer.animationKeys containsObject:ZA_ANIMATION_SHOW_KEY];

            // We want to remove the previous animations
            [self removeAnimationForKey:ZA_ANIMATION_SHOW_KEY];
            
            // Setup the animation values
            NSString *keypath = @"position.y";
            id currentValue = [NSNumber numberWithFloat:self.barView.layer.position.y];
            id midValue = [NSNumber numberWithFloat:self.barView.layer.position.y - 7];
            id finalValue = [NSNumber numberWithFloat:[self getOffscreenYPosition]];
            
            CAMediaTimingFunction *function = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseIn];
            NSArray *keyTimes = (shouldDrop ? @[@0.0,@0.3] : @[@0.0,@0.3,@0.5]);
            NSArray *values = (shouldDrop ? @[currentValue,finalValue] : @[currentValue,midValue,finalValue]);
            NSArray *timingFunctions = (shouldDrop ? @[function, function] : @[function, function, function]);
            
            // Get the duration. So we don't have to manually set it, this defaults to the final value in the animation keys.
            float duration = [[keyTimes objectAtIndex:(keyTimes.count - 1)] floatValue];
            
            // Perform the animation
            CAKeyframeAnimation *frameAnimation = [CAKeyframeAnimation animationWithKeyPath:keypath];
            [frameAnimation setValue:ZA_ANIMATION_DISMISS_KEY forKey:@"identifier"]; // So we can find identify animations later
//            [frameAnimation setCalculationMode:kCAAnimationLinear]; Defaults to Linear.
            [frameAnimation setKeyTimes:keyTimes];
            [frameAnimation setValues:values];
            [frameAnimation setTimingFunctions:timingFunctions];
            [frameAnimation setDuration:duration];
            [frameAnimation setRemovedOnCompletion:YES];
            [frameAnimation setDelegate:self];
            
            [self.barView.layer addAnimation:frameAnimation forKey:ZA_ANIMATION_DISMISS_KEY];
            
            CGPoint position = self.barView.layer.position;
            position.y = [finalValue floatValue];
            [self.barView.layer setPosition:position];
        }
    });
}

- (void) dismissFromTimer:(NSTimer *)timer {
    NSString *action = timer.userInfo;
    [ZAActivityBar dismissForAction:action];
}

///////////////////////////////////////////////////////////////

#pragma mark - Helpers

- (float) getOffscreenYPosition {
    return [self getHeight] + ((HEIGHT / 2) + PADDING);
}

- (float) getBarYPosition {
    return [self getHeight] - ((HEIGHT / 2) + PADDING) - self.offset;
}

// Returns the height for the bar in the current orientation
- (float) getHeight {
    UIInterfaceOrientation orientation = [[UIApplication sharedApplication] statusBarOrientation];
    float height = self.overlayWindow.frame.size.height;
    
    if (UIInterfaceOrientationIsLandscape(orientation)) {
        height = self.overlayWindow.frame.size.width;
    }
    
    return height;
}

///////////////////////////////////////////////////////////////

#pragma mark - Animation Methods / Helpers

// For some reason the SKBounceAnimation isn't removed if this method
// doesn't exist... Why?
- (void) animationDidStop:(CAAnimation *)anim finished:(BOOL)flag {
    
    // If the animation actually finished
    if (flag) {
        
        // Let's check if it's the right animation
        BOOL isDismiss = [[anim valueForKey:@"identifier"] isEqualToString:ZA_ANIMATION_DISMISS_KEY];
        if (isDismiss) {
            // Get rid of the stupid thing:
            [overlayWindow removeFromSuperview];
            overlayWindow = nil;
        }
    }
}

- (void) removeAnimationForKey:(NSString *)key {
    if ([self.barView.layer.animationKeys containsObject:key]) {
        CAAnimation *anim = [self.barView.layer animationForKey:key];
        
        // Find out how far into the animation we made it
        CFTimeInterval startTime = [[anim valueForKey:@"beginTime"] floatValue];
        CFTimeInterval pausedTime = [self.barView.layer convertTime:CACurrentMediaTime() fromLayer:nil];
        float diff = pausedTime - startTime;
        
        // We only need a ~rough~ frame, so it doesn't jump to the end position
        // and stays as close to in place as possible.
        int frame = (diff * 58.57 - 1); // 58fps?
        NSArray *frames = [anim valueForKey:@"values"];
        if (frame >= frames.count)  // For security
            frame = frames.count - 1;
        
        float yOffset = [[frames objectAtIndex:frame] floatValue];
        
        // And lets set that
        CGPoint position = self.barView.layer.position;
        position.y = yOffset;
        
        // We want to disable the implicit animation
        [CATransaction begin];
        [CATransaction setValue:(id)kCFBooleanTrue forKey:kCATransactionDisableActions];
        [self.barView.layer setPosition:position];
        [CATransaction commit];
        
        // And... actually remove it.
        [self.barView.layer removeAnimationForKey:key];
    }
}

///////////////////////////////////////////////////////////////

#pragma mark - Misc

- (void)drawRect:(CGRect)rect {
    
}

- (void)dealloc {
	self.fadeOutTimer = nil;
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)setFadeOutTimer:(NSTimer *)newTimer {
    
    if(fadeOutTimer)
        [fadeOutTimer invalidate], fadeOutTimer = nil;
    
    if(newTimer)
        fadeOutTimer = newTimer;
}

- (id)initWithFrame:(CGRect)frame {
	
    if ((self = [super initWithFrame:frame])) {
		self.userInteractionEnabled = NO;
        self.backgroundColor = [UIColor clearColor];
        self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        _isVisible = NO;
        _actionArray = [NSMutableArray new];
        _actionDict = [NSMutableDictionary new];
    }
	
    return self;
}

+ (ZAActivityBar *) sharedView {
    static dispatch_once_t once;
    static ZAActivityBar *sharedView;
    dispatch_once(&once, ^ { sharedView = [[ZAActivityBar alloc] initWithFrame:[[UIScreen mainScreen] bounds]]; });
    return sharedView;
}

- (void) registerNotifications {
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(positionBar:)
                                                 name:UIApplicationDidChangeStatusBarOrientationNotification
                                               object:nil];
}

- (void) positionOffscreen {
    CGPoint position = self.barView.layer.position;
    position.y = [self getOffscreenYPosition];
    [self.barView.layer setPosition:position];
}

- (void) positionBar:(NSNotification *)notification {

    double animationDuration = 0.7;
    
    UIInterfaceOrientation orientation = [[UIApplication sharedApplication] statusBarOrientation];

    CGFloat rotateAngle;
    CGRect frame = self.overlayWindow.frame;
    
    switch (orientation) {
        case UIInterfaceOrientationPortraitUpsideDown:
            rotateAngle = M_PI;
            break;
        case UIInterfaceOrientationLandscapeLeft:
            rotateAngle = -M_PI/2.0f;
            break;
        case UIInterfaceOrientationLandscapeRight:
            rotateAngle = M_PI/2.0f;
            break;
        default: // as UIInterfaceOrientationPortrait
            rotateAngle = 0.0;
            break;
    }
    
    if(notification) {
        [UIView animateWithDuration:animationDuration
                              delay:0
                            options:UIViewAnimationOptionAllowUserInteraction
                         animations:^{
                             [self positionWithFrame:frame angle:rotateAngle];
                        } completion:nil];
    } else {
        [self positionWithFrame:frame angle:rotateAngle];
    }
}

- (void) positionWithFrame:(CGRect)frame angle:(CGFloat)angle {
    self.overlayWindow.transform = CGAffineTransformMakeRotation(angle);
    self.overlayWindow.frame = frame;
}

///////////////////////////////////////////////////////////////

#pragma mark - Getters

- (UIView *) actionIndicatorView {
    
    if (!actionIndicatorView) {
        
        float size = HEIGHT / 2;
        CGRect rect = CGRectZero;
        rect.size = CGSizeMake(size, size);
        rect.origin.y = (HEIGHT - size) / 2;
        rect.origin.x = self.barView.bounds.size.width - size - rect.origin.y;
        
        actionIndicatorView = [UIView new];
        actionIndicatorView.alpha = 0.0f;
        actionIndicatorView.frame = rect;
        actionIndicatorView.autoresizingMask = (UIViewAutoresizingFlexibleLeftMargin);
        
        // Circle shape
        CAShapeLayer *pill = [CAShapeLayer new];
        pill.path = CGPathCreateWithEllipseInRect(actionIndicatorView.bounds, nil);
        pill.fillColor = [UIColor whiteColor].CGColor;
        [actionIndicatorView.layer addSublayer:pill];
    }
    
    if(!actionIndicatorView.superview)
        [self.barView addSubview:actionIndicatorView];

    return actionIndicatorView;
}

- (UILabel *) actionIndicatorLabel {
    if (!actionIndicatorLabel) {
        actionIndicatorLabel = [[UILabel alloc] initWithFrame:CGRectZero];
		actionIndicatorLabel.textColor = [UIColor blackColor];
		actionIndicatorLabel.backgroundColor = [UIColor clearColor];
		actionIndicatorLabel.adjustsFontSizeToFitWidth = YES;
		actionIndicatorLabel.textAlignment = UITextAlignmentCenter;
		actionIndicatorLabel.font = [UIFont boldSystemFontOfSize:12];
        actionIndicatorLabel.numberOfLines = 0;
    }
    
    if(!actionIndicatorLabel.superview)
        [self.actionIndicatorView addSubview:actionIndicatorLabel];
    
    return actionIndicatorLabel;
}

- (UILabel *)stringLabel {
    if (!stringLabel) {
        stringLabel = [[UILabel alloc] initWithFrame:CGRectZero];
		stringLabel.textColor = [UIColor whiteColor];
		stringLabel.backgroundColor = [UIColor clearColor];
		stringLabel.adjustsFontSizeToFitWidth = YES;
		stringLabel.textAlignment = UITextAlignmentLeft;
		stringLabel.baselineAdjustment = UIBaselineAdjustmentAlignCenters;
        stringLabel.autoresizingMask = (UIViewAutoresizingFlexibleWidth);
		stringLabel.font = [UIFont boldSystemFontOfSize:14];
		stringLabel.shadowColor = [UIColor blackColor];
		stringLabel.shadowOffset = CGSizeMake(0, -1);
        stringLabel.numberOfLines = 0;
    }
    
    if(!stringLabel.superview)
        [self.barView addSubview:stringLabel];
    
    return stringLabel;
}

- (UIWindow *)overlayWindow {
    if(!overlayWindow) {
        overlayWindow = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
        overlayWindow.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        overlayWindow.backgroundColor = [UIColor clearColor];
        overlayWindow.userInteractionEnabled = NO;
        overlayWindow.windowLevel = UIWindowLevelNormal; // UIWindowLevelAlert = infront of keyboard.
    }
    return overlayWindow;
}

- (UIView *)barView {
    if(!barView) {
        CGRect rect = CGRectMake(PADDING, FLT_MAX, self.overlayWindow.frame.size.width, HEIGHT);
        rect.size.width -= 2 * PADDING;
        rect.origin.y = [self getOffscreenYPosition];
        barView = [[UIView alloc] initWithFrame:rect];
        barView.layer.cornerRadius = 6;
		barView.backgroundColor = BAR_COLOR;
        barView.autoresizingMask = (UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleWidth);
        [self addSubview:barView];
    }
    return barView;
}

- (UIActivityIndicatorView *)spinnerView {
    if (!spinnerView) {
        spinnerView = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhite];
		spinnerView.hidesWhenStopped = YES;
		spinnerView.frame = CGRectMake(ICON_OFFSET, ICON_OFFSET, SPINNER_SIZE, SPINNER_SIZE);
    }
    
    if(!spinnerView.superview)
        [self.barView addSubview:spinnerView];
    
    return spinnerView;
}

- (UIImageView *)imageView {
    if (!imageView)
        imageView = [[UIImageView alloc] initWithFrame:CGRectMake(ICON_OFFSET, ICON_OFFSET, SPINNER_SIZE, SPINNER_SIZE)];
    
    if(!imageView.superview)
        [self.barView addSubview:imageView];
    
    return imageView;
}

@end
