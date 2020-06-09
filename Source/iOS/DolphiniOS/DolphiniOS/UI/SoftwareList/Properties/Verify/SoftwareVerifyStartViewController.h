// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "UICommon/GameFile.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareVerifyStartViewController : UITableViewController

@property(nonatomic) UICommon::GameFile* m_game_file;

@property (weak, nonatomic) IBOutlet UISwitch* m_crc32_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_md5_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_sha1_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_redump_switch;

@end

NS_ASSUME_NONNULL_END
