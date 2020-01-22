// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerGroupCheckboxCell.h"

@implementation ControllerGroupCheckboxCell

- (void)awakeFromNib
{
  [super awakeFromNib];
  // Initialization code
}

- (void)SetupCellWithSetting:(ControllerEmu::NumericSetting<bool>*)setting
{
  self.m_setting = setting;
  
  [self.m_name_label setText:DOLocalizedString([NSString stringWithUTF8String:setting->GetUIName()])];
  [self.m_switch setOn:setting->GetValue()];
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated
{
  [super setSelected:selected animated:animated];

  // Configure the view for the selected state
}

- (IBAction)SwitchChanged:(id)sender
{
  self.m_setting->SetValue([self.m_switch isOn]);
}

@end
