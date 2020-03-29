// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface GraphicsAdvancedViewController : UITableViewController
@property (weak, nonatomic) IBOutlet UILabel *m_wireframe_label;
@property (weak, nonatomic) IBOutlet UILabel *m_statistics_label;
@property (weak, nonatomic) IBOutlet UILabel *m_format_overlay_label;
@property (weak, nonatomic) IBOutlet UILabel *m_enable_api_validation_label;
@property (weak, nonatomic) IBOutlet UILabel *m_dump_textures_label;
@property (weak, nonatomic) IBOutlet UILabel *m_load_custom_textures_label;
@property (weak, nonatomic) IBOutlet UILabel *m_prefetch_custom_textures_label;
@property (weak, nonatomic) IBOutlet UILabel *m_dump_efb_target_label;
@property (weak, nonatomic) IBOutlet UILabel *m_disable_vram_copies_label;
@property (weak, nonatomic) IBOutlet UILabel *m_crop_label;
@property (weak, nonatomic) IBOutlet UILabel *m_progressive_scan_label;
@property (weak, nonatomic) IBOutlet UILabel *m_backend_multithreading_label;
@property (weak, nonatomic) IBOutlet UILabel *m_defer_efb_label;

@property (weak, nonatomic) IBOutlet UISwitch *m_wireframe_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_statistics_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_format_overlay_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_enable_api_validation_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_dump_textures_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_load_custom_textures_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_prefetch_custom_textures_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_dump_efb_target_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_disable_vram_copies_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_crop_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_progressive_scan_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_backend_multithreading_switch;
@property (weak, nonatomic) IBOutlet UISwitch *m_defer_efb_switch;

@end

NS_ASSUME_NONNULL_END
