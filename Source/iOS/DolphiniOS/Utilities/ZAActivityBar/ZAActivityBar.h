//
//  ZAActivityBar.h
//
//  Created by Zac Altman on 24/11/12.
//  Copyright (c) 2012 Zac Altman. All rights reserved.
//
//  Heavily influenced by SVProgressHUD by Sam Vermette
//  Pieces of code may have been directly copied.
//  Sam is a legend!
//  https://github.com/samvermette/SVProgressHUD
//

#import <UIKit/UIKit.h>

// Visual Properties
#define BAR_COLOR [[UIColor blackColor] colorWithAlphaComponent:0.8f]
#define HEIGHT 40.0f
#define PADDING 10.0f

// Best not to change these
#define SPINNER_SIZE 24.0f
#define ICON_OFFSET (HEIGHT - SPINNER_SIZE) / 2.0f
#define DEFAULT_ACTION @"defaultAction"

@interface ZAActivityBar : UIView

///////////////////////////////////////////////////////////////////////

// Properties

+ (void) setLocationBottom;
+ (void) setLocationTabBar;
+ (void) setLocationNavBar;

///////////////////////////////////////////////////////////////////////

// Basic use

+ (void) show;
+ (void) dismiss;

+ (void) showWithStatus:(NSString *)status;
+ (void) showSuccessWithStatus:(NSString *)status;
+ (void) showErrorWithStatus:(NSString *)status;
+ (void) showImage:(UIImage *)image status:(NSString *)status;

// Custom Durations
+ (void) showSuccessWithStatus:(NSString *)status duration:(NSTimeInterval)duration;
+ (void) showErrorWithStatus:(NSString *)status duration:(NSTimeInterval)duration;
+ (void) showImage:(UIImage *)image status:(NSString*)status duration:(NSTimeInterval)duration;

///////////////////////////////////////////////////////////////////////

// Advanced Use

+ (void) showForAction:(NSString *)action;
+ (void) dismissForAction:(NSString *)action;

+ (void) showWithStatus:(NSString *)status forAction:(NSString *)action;
+ (void) showSuccessWithStatus:(NSString *)status forAction:(NSString *)action;
+ (void) showErrorWithStatus:(NSString *)status forAction:(NSString *)action;
+ (void) showImage:(UIImage *)image status:(NSString *)status forAction:(NSString *)action;

// Custom Durations
+ (void) showSuccessWithStatus:(NSString *)status duration:(NSTimeInterval)duration forAction:(NSString *)action;
+ (void) showErrorWithStatus:(NSString *)status duration:(NSTimeInterval)duration forAction:(NSString *)action;
+ (void) showImage:(UIImage*)image status:(NSString*)status duration:(NSTimeInterval)duration forAction:(NSString *)action;

///////////////////////////////////////////////////////////////////////

@end
