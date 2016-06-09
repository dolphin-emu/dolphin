//
//  DolphinGameTableView.h
//  Dolphin
//
//  Created by Will Cobb on 5/31/16.
//  Copyright © 2016 Dolphin. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "AppDelegate.h"

@class DolphinGame;
@interface GameTableView : UITableViewController

@property (atomic, strong) DolphinGame * game;

@end
