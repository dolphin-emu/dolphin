// Copyright 2016 WillCobb, OatmealDome
// Licensed under GPLV2+
// Refer to the license.txt provided

#import <UIKit/UIKit.h>
#import "AppDelegate.h"

@class DolphinGame;
@interface GameTableView : UITableViewController

@property (atomic, strong) DolphinGame* game;

@end
