//
//  SKBounceAnimation.h
//  SKBounceAnimation
//
//  Created by Soroush Khanlou on 6/15/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>


@interface SKBounceAnimation : CAKeyframeAnimation

@property (nonatomic, retain) id fromValue;
@property (nonatomic, retain) id byValue;
@property (nonatomic, retain) id toValue;
@property (nonatomic, assign) NSUInteger numberOfBounces;
@property (nonatomic, assign) BOOL shouldOvershoot; //default YES
@property (nonatomic, assign) BOOL shake; //if shaking, set fromValue to the furthest value, and toValue to the current value


+ (SKBounceAnimation*) animationWithKeyPath:(NSString*)keyPath;


@end
