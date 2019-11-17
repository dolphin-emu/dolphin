// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareTableViewController : UITableViewController

@property(nonatomic) NSString* softwareDirectory;
@property(nonatomic) NSArray* softwareFiles;

@end

NS_ASSUME_NONNULL_END
