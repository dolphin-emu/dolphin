// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerGroupEnabledCell.h"

@implementation ControllerGroupEnabledCell

- (void)awakeFromNib
{
  [super awakeFromNib];
  // Initialization code
}

- (void)SetupCellWithGroup:(ControllerEmu::ControlGroup*)group
{
  self.m_group = group;
  
  [self.m_switch setOn:self.m_group->enabled];
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated
{
  [super setSelected:selected animated:animated];

  // Configure the view for the selected state
}

- (IBAction)SwitchChanged:(id)sender
{
  self.m_group->enabled = [self.m_switch isOn];
}

@end
