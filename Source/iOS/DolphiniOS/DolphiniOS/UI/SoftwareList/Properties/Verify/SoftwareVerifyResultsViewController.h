// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "DiscIO/VolumeVerifier.h"

#import "SoftwareVerifyResult.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareVerifyResultsViewController : UITableViewController

@property(nonatomic) SoftwareVerifyResult* m_result;

@end

NS_ASSUME_NONNULL_END
