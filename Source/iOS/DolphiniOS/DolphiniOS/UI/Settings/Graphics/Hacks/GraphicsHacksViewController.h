// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface GraphicsHacksViewController : UITableViewController

@property (weak, nonatomic) IBOutlet UILabel* m_skip_efb_cpu_label;
@property (weak, nonatomic) IBOutlet UILabel* m_ignore_format_changes_label;
@property (weak, nonatomic) IBOutlet UILabel* m_store_efb_copies_label;
@property (weak, nonatomic) IBOutlet UILabel* m_defer_efb_copies_label;
@property (weak, nonatomic) IBOutlet UILabel* m_accuracy_label;
@property (weak, nonatomic) IBOutlet UILabel* m_gpu_decoding_label;
@property (weak, nonatomic) IBOutlet UILabel* m_store_xfb_copies_label;
@property (weak, nonatomic) IBOutlet UILabel* m_immediate_xfb_label;
@property (weak, nonatomic) IBOutlet UILabel* m_fast_depth_label;
@property (weak, nonatomic) IBOutlet UILabel* m_bbox_label;
@property (weak, nonatomic) IBOutlet UILabel* m_vertex_rounding_label;
@property (weak, nonatomic) IBOutlet UILabel* m_save_texture_cache_label;

@property (weak, nonatomic) IBOutlet UISwitch* m_skip_efb_cpu_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_ignore_format_changes_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_store_efb_copies_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_defer_efb_copies_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_gpu_decoding_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_store_xfb_copies_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_immediate_xfb_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_fast_depth_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_bbox_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_vertex_rounding_switch;
@property (weak, nonatomic) IBOutlet UISwitch* m_save_texture_cache_switch;

@property (weak, nonatomic) IBOutlet UISlider* m_accuracy_slider;

@end

NS_ASSUME_NONNULL_END
