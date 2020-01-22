// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerExtensionsViewController.h"

#import "ControllerExtensionCell.h"
#import "ControllerSettingsUtils.h"

@interface ControllerExtensionsViewController ()

@end

@implementation ControllerExtensionsViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  
  [ControllerSettingsUtils SaveSettings:true];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
  return self.m_attachments->GetAttachmentList().size();
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath*)indexPath
{
  ControllerExtensionCell* cell = (ControllerExtensionCell*)[tableView dequeueReusableCellWithIdentifier:@"extension_cell" forIndexPath:indexPath];
  
  const std::unique_ptr<ControllerEmu::EmulatedController>& controller = self.m_attachments->GetAttachmentList().at(indexPath.row);
  [cell.m_extension_name setText:DOLocalizedString([NSString stringWithUTF8String:controller->GetDisplayName().c_str()])];
  
  if (self.m_attachments->GetSelectedAttachment() == indexPath.row)
  {
    [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  }
  
  return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  UITableViewCell* old_cell = [tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:self.m_attachments->GetSelectedAttachment() inSection:0]];
  [old_cell setAccessoryType:UITableViewCellAccessoryNone];
  
  self.m_attachments->SetSelectedAttachment(static_cast<u32>(indexPath.row));
  
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
