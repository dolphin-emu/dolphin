// Copyright 2016 WillCobb, OatmealDome
// Licensed under GPLV2+
// Refer to the license.txt provided

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

FOUNDATION_EXPORT NSString* const DolphinGameSaveStatesChangedNotification;

@interface DolphinGame : NSObject

@property (strong, nonatomic) NSString*		path;
@property (nonatomic, readonly) NSString*	title;
@property (nonatomic, readonly) NSString*	gameTitle;
@property (nonatomic, readonly) NSString*	gameSubtitle;
@property (nonatomic, readonly) UIImage*	banner;

+ (int)preferredLanguage;
+ (NSArray*)gamesAtPath:(NSString*)path;
- (DolphinGame*)initWithPath:(NSString*)path;

@end
