// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "UICommon/GameFile.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SoftwareInfoViewController : UITableViewController

@property(nonatomic) UICommon::GameFile* m_game_file;

@property (weak, nonatomic) IBOutlet UILabel *m_name_label;
@property (weak, nonatomic) IBOutlet UILabel *m_game_id_label;
@property (weak, nonatomic) IBOutlet UILabel *m_country_label;
@property (weak, nonatomic) IBOutlet UILabel *m_maker_label;
@property (weak, nonatomic) IBOutlet UILabel *m_apploader_label;

@end

NS_ASSUME_NONNULL_END
