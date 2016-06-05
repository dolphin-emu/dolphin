//
//  AppDelegate.h
//  DolphiniOS
//
//  Created by Will Cobb on 5/20/16.
//
//

#import <UIKit/UIKit.h>

#import "WCEasySettings.h"
@class EmulatorViewController;
@class DolphinGame;
@interface AppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic, getter=getSettingsViewController) WCEasySettingsViewController *settingsViewController;
@property EmulatorViewController    * currentEmulationController;

+ (AppDelegate *)sharedInstance;

- (NSString *)cheatsDir;
- (NSString *)batteryDir;
- (NSString *)documentsPath;

- (void)startGame:(DolphinGame *)game;

@end

