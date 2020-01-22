// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerGroupButtonCell.h"

#import "InputCommon/ControllerInterface/ControllerInterface.h"

#import "ControllerSettingsUtils.h"

@implementation ControllerGroupButtonCell

- (void)awakeFromNib
{
  [super awakeFromNib];
}

- (void)SetupCellWithControl:(std::unique_ptr<ControllerEmu::Control>&)control controller:(ControllerEmu::EmulatedController*)controller
{
  self.m_controller = controller;
  self.m_reference = control->control_ref.get();
  
  [self.m_button_name setText:DOLocalizedString([NSString stringWithUTF8String:control->ui_name.c_str()])];
  
  [self ResetToDefault];
}

- (void)ResetToDefault
{
  NSString* current_expression = [NSString stringWithUTF8String:self.m_reference->GetExpression().c_str()];
  if ([current_expression isEqualToString:@""])
  {
    current_expression = NSLocalizedString(@"[Not Set]", nil);
  }
  
  [self.m_user_setting_label setText:[ControllerSettingsUtils FormatExpression:current_expression]];
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated
{
  [super setSelected:selected animated:animated];
  
  if (!selected)
  {
    return;
  }
}


@end
