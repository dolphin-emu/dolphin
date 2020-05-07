// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "Core/Boot/Boot.h"

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface ReloadStateNoticeViewController : UIViewController
{
  std::unique_ptr<BootParameters> m_boot_parameters;
}

@property (weak, nonatomic) IBOutlet UILabel* m_game_name_label;

@end

NS_ASSUME_NONNULL_END
