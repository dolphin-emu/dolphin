// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SettingsDebugViewController : UITableViewController

@property (weak, nonatomic) IBOutlet UISwitch* m_load_store_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_load_store_fp_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_load_store_p_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_fp_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_integer_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_p_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_system_registers_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_branch_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_register_cache_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_skip_idle_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_fastmem_switch;
@property (weak, nonatomic) IBOutlet UITableViewCell* m_set_debugged_cell;
@property (weak, nonatomic) IBOutlet UILabel* m_set_debugged_label;

@end

NS_ASSUME_NONNULL_END
