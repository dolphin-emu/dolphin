// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "InputCommon/ControllerEmu/ControllerEmu.h"

NS_ASSUME_NONNULL_BEGIN

@interface ControllerDetailsViewController : UITableViewController

@property(nonatomic) ControllerEmu::EmulatedController* m_controller;
@property(nonatomic) int m_port;
@property(nonatomic) bool m_is_wii;

@property (weak, nonatomic) IBOutlet UINavigationItem *m_nav_item;

@property(weak, nonatomic) IBOutlet UILabel* m_type_label;
@property (weak, nonatomic) IBOutlet UILabel* m_device_label;

@property (weak, nonatomic) IBOutlet UITableViewCell* m_device_cell;
@property (weak, nonatomic) IBOutlet UITableViewCell* m_configure_cell;
@property (weak, nonatomic) IBOutlet UITableViewCell* m_load_profile_cell;
@property (weak, nonatomic) IBOutlet UITableViewCell* m_save_profile_cell;

@property (weak, nonatomic) IBOutlet UILabel* m_device_header_label;
@property (weak, nonatomic) IBOutlet UILabel* m_device_configure_label;
@property (weak, nonatomic) IBOutlet UILabel* m_load_profile_label;
@property (weak, nonatomic) IBOutlet UILabel* m_save_profile_label;

@end

NS_ASSUME_NONNULL_END
