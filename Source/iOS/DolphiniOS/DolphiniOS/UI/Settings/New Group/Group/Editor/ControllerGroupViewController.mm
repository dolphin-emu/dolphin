// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerGroupViewController.h"

#import "InputCommon/ControllerEmu/Control/Control.h"
#import "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#import "InputCommon/ControllerInterface/ControllerInterface.h"
#import "InputCommon/ControllerInterface/Device.h"
#import "InputCommon/ControlReference/ControlReference.h"

#import "ControllerGroupButtonCell.h"
#import "ControllerGroupCheckboxCell.h"
#import "ControllerGroupDoubleCell.h"
#import "ControllerSettingsUtils.h"

@interface ControllerGroupViewController ()

@end

@implementation ControllerGroupViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  ControllerEmu::ControlGroup* group = self.m_control_group;
  
  self.m_need_indicator = group->type == ControllerEmu::GroupType::Cursor ||
                          group->type == ControllerEmu::GroupType::Stick ||
                          group->type == ControllerEmu::GroupType::Tilt ||
                          group->type == ControllerEmu::GroupType::MixedTriggers ||
                          group->type == ControllerEmu::GroupType::Force ||
                          group->type == ControllerEmu::GroupType::IMUAccelerometer ||
                          group->type == ControllerEmu::GroupType::IMUGyroscope ||
                          group->type == ControllerEmu::GroupType::Shake;

  self.m_need_calibration = group->type == ControllerEmu::GroupType::Cursor ||
                            group->type == ControllerEmu::GroupType::Stick ||
                            group->type == ControllerEmu::GroupType::Tilt ||
                            group->type == ControllerEmu::GroupType::Force;
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  
  [ControllerSettingsUtils SaveSettings:self.m_is_wii];
  
  if (self->m_input_wait_block != nil)
  {
    dispatch_block_cancel(self->m_input_wait_block);
  }
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
  // TODO: Enabled toggle
  // TODO: Indicators
  // TODO: Calibration
  
  return 2;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  switch (section)
  {
    case 0: // Buttons
      return self.m_control_group->controls.size();
    case 1: // Numeric Settings
      return self.m_control_group->numeric_settings.size();
    default:
      return 0;
  }
  
  return self.m_control_group->controls.size();
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  // TODO: Enabled toggle
  // TODO: Indicators
  // TODO: Calibration
  
  if (indexPath.section == 0)
  {
    ControllerGroupButtonCell* cell = (ControllerGroupButtonCell*)[tableView dequeueReusableCellWithIdentifier:@"button_cell" forIndexPath:indexPath];
    [cell SetupCellWithControl:self.m_control_group->controls[indexPath.row] controller:self.m_controller];
    
    return cell;
  }
  else if (indexPath.section == 1)
  {
    std::unique_ptr<ControllerEmu::NumericSettingBase>& setting = self.m_control_group->numeric_settings[indexPath.row];
    ControllerEmu::SettingType setting_type = setting->GetType();
    
    if (setting_type == ControllerEmu::SettingType::Double)
    {
      ControllerGroupDoubleCell* cell = (ControllerGroupDoubleCell*)[tableView dequeueReusableCellWithIdentifier:@"double_cell" forIndexPath:indexPath];
      [cell SetupCellWithSetting:static_cast<ControllerEmu::NumericSetting<double>*>(setting.get())];
      [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
      
      return cell;
    }
    else if (setting_type == ControllerEmu::SettingType::Bool)
    {
      ControllerGroupCheckboxCell* cell = (ControllerGroupCheckboxCell*)[tableView dequeueReusableCellWithIdentifier:@"bool_cell" forIndexPath:indexPath];
      [cell SetupCellWithSetting:static_cast<ControllerEmu::NumericSetting<bool>*>(setting.get())];
      [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
      
      return cell;
    }
  }
  
  return nil;
}

- (NSIndexPath*)tableView:(UITableView*)tableView willSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if ([self.tableView indexPathForSelectedRow] != nil || indexPath.section == 1)
  {
    return nil;
  }
  
  return indexPath;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 0)
  {
    ControllerGroupButtonCell* cell = (ControllerGroupButtonCell*)[self.tableView cellForRowAtIndexPath:indexPath];
    
    [cell.m_user_setting_label setText:@"[...]"];
    
    self->m_input_wait_block = dispatch_block_create((dispatch_block_flags_t)0, ^{
      dispatch_sync(dispatch_get_main_queue(), ^{
        [cell.m_user_setting_label setText:@"[...]"];
        
        [self.tableView setUserInteractionEnabled:false];
        
        [self.m_cancel_item setEnabled:true];
        [self.m_clear_item setEnabled:true];
      });
      
      const auto [device, input] = g_controller_interface.DetectInput(3000, {self.m_controller->GetDefaultDevice().ToString()});
        
      if (!input)
      {
        dispatch_sync(dispatch_get_main_queue(), ^{
          [self StopEditingCell:cell WithCancelBlock:false];
        });
        
        return;
      }
      
      std::string expression = "`" + input->GetName() + "`";
      cell.m_reference->SetExpression(expression);
      cell.m_controller->UpdateSingleControlReference(g_controller_interface, cell.m_reference);
      
      dispatch_sync(dispatch_get_main_queue(), ^{
        [self StopEditingCell:cell WithCancelBlock:false];
      });
      
      self->m_input_wait_block = nil;
    });
    
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), self->m_input_wait_block);
    
    return;
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

- (ControllerGroupButtonCell*)GetSelectedButtonCell
{
  NSIndexPath* index_path = [self.tableView indexPathForSelectedRow];
  
  if (index_path.section != 0)
  {
    return nil;
  }
  
  return (ControllerGroupButtonCell*)[self.tableView cellForRowAtIndexPath:index_path];
}

- (IBAction)CancelPressed:(id)sender
{
  [self StopEditingCell:[self GetSelectedButtonCell] WithCancelBlock:true];
}

- (IBAction)ClearPressed:(id)sender
{
  ControllerGroupButtonCell* cell = [self GetSelectedButtonCell];
  
  cell.m_reference->SetExpression("");
  cell.m_controller->UpdateSingleControlReference(g_controller_interface, cell.m_reference);
  
  [self StopEditingCell:cell WithCancelBlock:true];
}

- (IBAction)ResetPressed:(id)sender
{
  // TODO
  [self StopEditingCell:[self GetSelectedButtonCell] WithCancelBlock:true];
}

- (void)StopEditingCell:(ControllerGroupButtonCell*)cell WithCancelBlock:(bool)cancel_block
{
  [cell ResetToDefault];
  
  [self.tableView deselectRowAtIndexPath:[self.tableView indexPathForCell:cell] animated:true];
  
  if (self->m_input_wait_block != nil && cancel_block)
  {
    dispatch_block_cancel(self->m_input_wait_block);
    self->m_input_wait_block = nil;
  }
  
  [self.tableView setUserInteractionEnabled:true];
  
  [self.m_cancel_item setEnabled:false];
  [self.m_clear_item setEnabled:false];
}

@end
