//
//  SKBounceAnimation.m
//  SKBounceAnimation
//
//  Created by Soroush Khanlou on 6/15/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "SKBounceAnimation.h"

/*
 Keypaths:
 
 Float animations:
 anchorPoint
 cornerRadius
 borderWidth
 opacity
 shadowRadius
 shadowOpacity
 zPosition
 
 Point/size animations:
 position
 shadowOffset
 
 Rect animations:
 bounds
 frame - not strictly animatable, use bounds
 contentsRect
 
 Colors:
 backgroundColor
 borderColor
 shadowColor
 
 CATransform3D:
 transform
 
 
 Meaningless:
 backgroundFilters
 compositingFilter
 contents
 doubleSided
 filters
 hidden
 mask
 masksToBounds
 sublayers
 sublayerTransform
 
 */

@interface SKBounceAnimation (Private)

- (void) createValueArray;
- (NSArray*) valueArrayForStartValue:(CGFloat)startValue endValue:(CGFloat)endValue;
- (CGPathRef) createPathFromXValues:(NSArray*)xValues yValues:(NSArray*)yValues;
- (NSArray*) createRectArrayFromXValues:(NSArray*)xValues yValues:(NSArray*)yValues widths:(NSArray*)widths heights:(NSArray*)heights;
- (NSArray*) createColorArrayFromRed:(NSArray*)redValues green:(NSArray*)greenValues blue:(NSArray*)blueValues alpha:(NSArray*)alphaValues;
- (NSArray*) createTransformArrayFromM11:(NSArray*)m11 M12:(NSArray*)m12 M13:(NSArray*)m13 M14:(NSArray*)m14
							  M21:(NSArray*)m21 M22:(NSArray*)m22 M23:(NSArray*)m23 M14:(NSArray*)m24
							  M31:(NSArray*)m31 M32:(NSArray*)m32 M33:(NSArray*)m33 M14:(NSArray*)m34
							  M41:(NSArray*)m41 M42:(NSArray*)m42 M43:(NSArray*)m43 M14:(NSArray*)m44;

@end

@implementation SKBounceAnimation

@synthesize fromValue, byValue, toValue, numberOfBounces, shouldOvershoot;

+ (SKBounceAnimation*) animationWithKeyPath:(NSString*)keyPath {
	return [[self alloc] initWithKeyPath:keyPath];
}

- (id) initWithKeyPath:(NSString*)keyPath {
	self = [super init];
	if (self) {
		super.keyPath = keyPath;
		self.numberOfBounces = 2;
		self.shouldOvershoot = YES;
	}
	return self;
}

- (void) setFromValue:(id)newFromValue {
	[super setValue:newFromValue forKey:@"fromValueKey"];
	[self createValueArray];
}

- (void) setByValue:(id)newByValue {
	[super setValue:newByValue forKey:@"byValueKey"];
	//don't know if this is to spec
	self.toValue = [NSNumber numberWithFloat:[self.fromValue floatValue] + [self.byValue floatValue]];
	[self createValueArray];
}

- (void) setToValue:(id)newToValue {
	[super setValue:newToValue forKey:@"toValueKey"];
	[self createValueArray];
}

- (void) setDuration:(CFTimeInterval)newDuration {
	[super setDuration:newDuration];
	[self createValueArray];
}

- (void) setNumberOfBounces:(NSUInteger)newNumberOfBounces {
	[super setValue:[NSNumber numberWithUnsignedInt:newNumberOfBounces] forKey:@"numBounces"];
	[self createValueArray];
}

- (NSUInteger) numberOfBounces {
	return [[super valueForKey:@"numBounces"] unsignedIntValue];
}

- (void) setShouldOvershoot:(BOOL)newShouldOvershoot {
	[super setValue:[NSNumber numberWithBool:newShouldOvershoot] forKey:@"shouldOvershootKey"];
	[self createValueArray];
}

- (BOOL) shouldOvershoot {
	return [[super valueForKey:@"shouldOvershootKey"] boolValue];
}

- (void) setShake:(BOOL)newShake {
	[super setValue:[NSNumber numberWithBool:newShake] forKey:@"shakeKey"];
	[self createValueArray];
}

- (BOOL) shake {
	return [[super valueForKey:@"shakeKey"] boolValue];
}

- (id) fromValue {
	return [super valueForKey:@"fromValueKey"];
}

- (id) byValue {
	return [super valueForKey:@"byValueKey"];
}

- (id) toValue {
	return [super valueForKey:@"toValueKey"];
}

- (void) createValueArray {
	if (self.fromValue && self.toValue && self.duration) {
		if ([self.fromValue isKindOfClass:[NSNumber class]] && [self.toValue isKindOfClass:[NSNumber class]]) {
			self.values = [self valueArrayForStartValue:[self.fromValue floatValue] endValue:[self.toValue floatValue]];
		} else if ([self.fromValue isKindOfClass:[UIColor class]] && [self.toValue isKindOfClass:[UIColor class]]) {
			const CGFloat *fromComponents = CGColorGetComponents(((UIColor*)self.fromValue).CGColor);
			const CGFloat *toComponents = CGColorGetComponents(((UIColor*)self.toValue).CGColor);
			//			NSLog(@"thing");
			//			NSLog(@"from %0.2f %0.2f %0.2f %0.2f", fromComponents[0], fromComponents[1], fromComponents[2], fromComponents[3]);
			//			NSLog(@"to %0.2f %0.2f %0.2f %0.2f", toComponents[0], toComponents[1], toComponents[2], toComponents[3]);
			self.values = [self createColorArrayFromRed:
						[self valueArrayForStartValue:fromComponents[0] endValue:toComponents[0]]
										   green:
						[self valueArrayForStartValue:fromComponents[1] endValue:toComponents[1]]
										    blue:
						[self valueArrayForStartValue:fromComponents[2] endValue:toComponents[2]]
										   alpha:
						[self valueArrayForStartValue:fromComponents[3] endValue:toComponents[3]]];
		} else if ([self.fromValue isKindOfClass:[NSValue class]] && [self.toValue isKindOfClass:[NSValue class]]) {
			NSString *valueType = [NSString stringWithCString:[self.fromValue objCType] encoding:NSStringEncodingConversionAllowLossy];
			if ([valueType rangeOfString:@"CGRect"].location == 1) {
				CGRect fromRect = [self.fromValue CGRectValue];
				CGRect toRect = [self.toValue CGRectValue];
				self.values = [self createRectArrayFromXValues:
							[self valueArrayForStartValue:fromRect.origin.x endValue:toRect.origin.x]
											    yValues:
							[self valueArrayForStartValue:fromRect.origin.y endValue:toRect.origin.y]
												widths:
							[self valueArrayForStartValue:fromRect.size.width endValue:toRect.size.width]
											    heights:
							[self valueArrayForStartValue:fromRect.size.height endValue:toRect.size.height]];
				
			} else if ([valueType rangeOfString:@"CGPoint"].location == 1) {
				CGPoint fromPoint = [self.fromValue CGPointValue];
				CGPoint toPoint = [self.toValue CGPointValue];
				CGPathRef path = createPathFromXYValues([self valueArrayForStartValue:fromPoint.x endValue:toPoint.x], [self valueArrayForStartValue:fromPoint.y endValue:toPoint.y]);
				self.path = path;
				CGPathRelease(path);
				
			} else if ([valueType rangeOfString:@"CATransform3D"].location == 1) {
				CATransform3D fromTransform = [self.fromValue CATransform3DValue];
				CATransform3D toTransform = [self.toValue CATransform3DValue];
				
				//				NSLog(@"from [%0.2f %0.2f %0.2f %0.2f; %0.2f %0.2f %0.2f %0.2f; %0.2f %0.2f %0.2f %0.2f; %0.2f %0.2f %0.2f %0.2f]", fromTransform.m11, fromTransform.m12, fromTransform.m13, fromTransform.m14, fromTransform.m21, fromTransform.m22, fromTransform.m23, fromTransform.m24, fromTransform.m31, fromTransform.m32, fromTransform.m33, fromTransform.m34, fromTransform.m41, fromTransform.m42, fromTransform.m43, fromTransform.m44);
				//
				//				NSLog(@"to [%0.2f %0.2f %0.2f %0.2f; %0.2f %0.2f %0.2f %0.2f; %0.2f %0.2f %0.2f %0.2f; %0.2f %0.2f %0.2f %0.2f]", toTransform.m11, toTransform.m12, toTransform.m13, toTransform.m14, toTransform.m21, toTransform.m22, toTransform.m23, toTransform.m24, toTransform.m31, toTransform.m32, toTransform.m33, toTransform.m34, toTransform.m41, toTransform.m42, toTransform.m43, toTransform.m44);
				
				self.values = [self createTransformArrayFromM11:
							[self valueArrayForStartValue:fromTransform.m11 endValue:toTransform.m11]
												    M12:
							[self valueArrayForStartValue:fromTransform.m12 endValue:toTransform.m12]
												    M13:
							[self valueArrayForStartValue:fromTransform.m13 endValue:toTransform.m13]
												    M14:
							[self valueArrayForStartValue:fromTransform.m14 endValue:toTransform.m14]
												    M21:
							[self valueArrayForStartValue:fromTransform.m21 endValue:toTransform.m21]
												    M22:
							[self valueArrayForStartValue:fromTransform.m22 endValue:toTransform.m22]
												    M23:
							[self valueArrayForStartValue:fromTransform.m23 endValue:toTransform.m23]
												    M24:
							[self valueArrayForStartValue:fromTransform.m24 endValue:toTransform.m24]
												    M31:
							[self valueArrayForStartValue:fromTransform.m31 endValue:toTransform.m31]
												    M32:
							[self valueArrayForStartValue:fromTransform.m32 endValue:toTransform.m32]
												    M33:
							[self valueArrayForStartValue:fromTransform.m33 endValue:toTransform.m33]
												    M34:
							[self valueArrayForStartValue:fromTransform.m34 endValue:toTransform.m34]
												    M41:
							[self valueArrayForStartValue:fromTransform.m41 endValue:toTransform.m41]
												    M42:
							[self valueArrayForStartValue:fromTransform.m42 endValue:toTransform.m42]
												    M43:
							[self valueArrayForStartValue:fromTransform.m43 endValue:toTransform.m43]
												    M44:
							[self valueArrayForStartValue:fromTransform.m44 endValue:toTransform.m44]
							];
			} else if ([valueType rangeOfString:@"CGSize"].location == 1) {
				CGSize fromSize = [self.fromValue CGSizeValue];
				CGSize toSize = [self.toValue CGSizeValue];
				CGPathRef path = self.path =
				createPathFromXYValues([self valueArrayForStartValue:fromSize.width endValue:toSize.width],
								   [self valueArrayForStartValue:fromSize.height endValue:toSize.height]);
				CGPathRelease(path);
			}
			
		}
		self.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear];
	}
}

- (NSArray*) createRectArrayFromXValues:(NSArray*)xValues yValues:(NSArray*)yValues widths:(NSArray*)widths heights:(NSArray*)heights {
	NSAssert(xValues.count == yValues.count && xValues.count == widths.count && xValues.count == heights.count, @"array must have arrays of equal size");
	
	NSUInteger numberOfRects = xValues.count;
	NSMutableArray *values = [NSMutableArray arrayWithCapacity:numberOfRects];
	CGRect value;
	
	for (int i = 1; i < numberOfRects; i++) {
		value = CGRectMake(
					    [[xValues objectAtIndex:i] floatValue],
					    [[yValues objectAtIndex:i] floatValue],
					    [[widths objectAtIndex:i] floatValue],
					    [[heights objectAtIndex:i] floatValue]
					    );
		[values addObject:[NSValue valueWithCGRect:value]];
	}
	return values;
}

static CGPathRef createPathFromXYValues(NSArray *xValues, NSArray *yValues) {
	NSUInteger numberOfPoints = xValues.count;
	CGMutablePathRef path = CGPathCreateMutable();
	CGPoint value;
	value = CGPointMake([[xValues objectAtIndex:0] floatValue], [[yValues objectAtIndex:0] floatValue]);
	CGPathMoveToPoint(path, NULL, value.x, value.y);
	
	for (int i = 1; i < numberOfPoints; i++) {
		value = CGPointMake([[xValues objectAtIndex:i] floatValue], [[yValues objectAtIndex:i] floatValue]);
		CGPathAddLineToPoint(path, NULL, value.x, value.y);
	}
	return path;
}

- (NSArray*) createTransformArrayFromM11:(NSArray*)m11 M12:(NSArray*)m12 M13:(NSArray*)m13 M14:(NSArray*)m14
							  M21:(NSArray*)m21 M22:(NSArray*)m22 M23:(NSArray*)m23 M24:(NSArray*)m24
							  M31:(NSArray*)m31 M32:(NSArray*)m32 M33:(NSArray*)m33 M34:(NSArray*)m34
							  M41:(NSArray*)m41 M42:(NSArray*)m42 M43:(NSArray*)m43 M44:(NSArray*)m44 {
	NSUInteger numberOfTransforms = m11.count;
	NSMutableArray *values = [NSMutableArray arrayWithCapacity:numberOfTransforms];
	CATransform3D value;
	
	for (int i = 1; i < numberOfTransforms; i++) {
		value = CATransform3DIdentity;
		value.m11 = [[m11 objectAtIndex:i] floatValue];
		value.m12 = [[m12 objectAtIndex:i] floatValue];
		value.m13 = [[m13 objectAtIndex:i] floatValue];
		value.m14 = [[m14 objectAtIndex:i] floatValue];
		
		value.m21 = [[m21 objectAtIndex:i] floatValue];
		value.m22 = [[m22 objectAtIndex:i] floatValue];
		value.m23 = [[m23 objectAtIndex:i] floatValue];
		value.m24 = [[m24 objectAtIndex:i] floatValue];
		
		value.m31 = [[m31 objectAtIndex:i] floatValue];
		value.m32 = [[m32 objectAtIndex:i] floatValue];
		value.m33 = [[m33 objectAtIndex:i] floatValue];
		value.m44 = [[m34 objectAtIndex:i] floatValue];
		
		value.m41 = [[m41 objectAtIndex:i] floatValue];
		value.m42 = [[m42 objectAtIndex:i] floatValue];
		value.m43 = [[m43 objectAtIndex:i] floatValue];
		value.m44 = [[m44 objectAtIndex:i] floatValue];
		
		[values addObject:[NSValue valueWithCATransform3D:value]];
	}
	return values;
}

- (NSArray*) createColorArrayFromRed:(NSArray*)redValues green:(NSArray*)greenValues blue:(NSArray*)blueValues alpha:(NSArray*)alphaValues {
	NSAssert(redValues.count == blueValues.count && redValues.count == greenValues.count && redValues.count == alphaValues.count, @"array must have arrays of equal size");
	
	NSUInteger numberOfColors = redValues.count;
	NSMutableArray *values = [NSMutableArray arrayWithCapacity:numberOfColors];
	UIColor *value;
	
	for (int i = 1; i < numberOfColors; i++) {
		value = [UIColor colorWithRed:[[redValues objectAtIndex:i] floatValue]
						    green:[[greenValues objectAtIndex:i] floatValue]
							blue:[[blueValues objectAtIndex:i] floatValue]
						    alpha:[[alphaValues objectAtIndex:i] floatValue]];
		//		NSLog(@"a color %@", value);
		[values addObject:(id)value.CGColor];
	}
	return values;
}

- (NSArray*) valueArrayForStartValue:(CGFloat)startValue endValue:(CGFloat)endValue {
	int steps = 60*self.duration; //60 fps desired
	
	CGFloat alpha = 0;
	if (startValue == endValue) {
		alpha = log2f(0.1f)/steps;
	} else {
		alpha = log2f(0.1f/fabs(endValue - startValue))/steps;
	}
	if (alpha > 0) {
		alpha = -1.0f*alpha;
	}
	CGFloat numberOfPeriods = self.numberOfBounces/2 + 0.5;
	CGFloat omega = numberOfPeriods * 2*M_PI/steps;
	
	//uncomment this to get the equation of motion
	//	NSLog(@"y = %0.2f * e^(%0.5f*x)*cos(%0.10f*x) + %0.0f over %d frames", startValue - endValue, alpha, omega, endValue, steps);
	
	NSMutableArray *values = [NSMutableArray arrayWithCapacity:steps];
	CGFloat value = 0;
	
	CGFloat sign = (endValue-startValue)/fabsf(endValue-startValue);
	
	CGFloat oscillationComponent;
	CGFloat coefficient;
	
	// conforms to y = A * e^(-alpha*t)*cos(omega*t)
	for (int t = 0; t < steps; t++) {
		//decaying mass-spring-damper solution with initial displacement
		
		if (self.shake) {
			oscillationComponent =  sin(omega*t);
		} else {
			oscillationComponent =  cos(omega*t);
		}
		
		if (self.shouldOvershoot) {
			coefficient =  (startValue - endValue);
		} else {
			coefficient = -1*sign*fabsf((startValue - endValue));
		}
		
		value = coefficient * pow(2.71, alpha*t) * oscillationComponent + endValue;
		
		
		
		[values addObject:[NSNumber numberWithFloat:value]];
	}
	return values;
}


@end

