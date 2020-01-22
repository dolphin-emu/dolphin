// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerGroupDoubleCell.h"

@implementation ControllerGroupDoubleCell

- (void)awakeFromNib
{
  [super awakeFromNib];
  
  // TODO: Couldn't do this in the storyboard for some reason?
  self.m_text_field.delegate = self;
}

- (void)SetupCellWithSetting:(ControllerEmu::NumericSetting<double>*)setting
{
  self.m_setting = setting;
  
  [self.m_name_label setText:DOLocalizedString([NSString stringWithUTF8String:setting->GetUIName()])];
  [self.m_text_field setText:[NSString localizedStringWithFormat:@"%f", setting->GetValue()]];
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated
{
  [super setSelected:selected animated:animated];

  // Configure the view for the selected state
}

- (IBAction)ValueChanged:(id)sender
{
  self.m_setting->SetValue([self.m_text_field.text doubleValue]);
}

- (BOOL)textFieldShouldReturn:(UITextField*)text_field
{
    [text_field resignFirstResponder];
    return true;
}


@end
