//
//  DolphinGameTableView.h
//  Dolphin
//
//  Created by Will Cobb on 5/31/16.
//  Copyright 2016 Dolphin Emulator Project
//  Licensed under GPLv2+
//  Refer to the license.txt file included.

#import <UIKit/UIKit.h>
#import "AppDelegate.h"

@class DolphinGame;
@interface GameTableView : UITableViewController

@property (atomic, strong) DolphinGame * game;

@end
