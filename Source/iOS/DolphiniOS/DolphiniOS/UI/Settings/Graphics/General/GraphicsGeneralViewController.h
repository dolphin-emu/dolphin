// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface GraphicsGeneralViewController : UITableViewController

@property (weak, nonatomic) IBOutlet UILabel* m_backend_label;
@property (weak, nonatomic) IBOutlet UILabel* m_aspect_ratio_label;
@property (weak, nonatomic) IBOutlet UILabel* m_vsync_label;
@property (weak, nonatomic) IBOutlet UILabel* m_fps_label;
@property (weak, nonatomic) IBOutlet UILabel* m_netplay_messages_label;
@property (weak, nonatomic) IBOutlet UILabel* m_render_time_log_label;
@property (weak, nonatomic) IBOutlet UILabel* m_netplay_ping_label;
@property (weak, nonatomic) IBOutlet UILabel* m_mode_label;
@property (weak, nonatomic) IBOutlet UILabel* m_compile_shaders_label;

@property (weak, nonatomic) IBOutlet UISwitch* m_vsync_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_fps_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_netplay_messages_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_render_time_log_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_netplay_ping_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_compile_shaders_switch;

@end

NS_ASSUME_NONNULL_END
