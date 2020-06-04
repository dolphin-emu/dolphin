// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "NPSJoinViewController.h"

#import "Core/Config/NetplaySettings.h"

@interface NPSJoinViewController ()

@end

@implementation NPSJoinViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}
- (IBAction)HostCodeChanged:(UITextField*)sender
{
  
}

- (CGFloat)tableView:(UITableView*)tableView heightForRowAtIndexPath:(NSIndexPath*)indexPath
{
  std::string choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
  if (choice == "traversal" && indexPath.section == 1)
  {
    return CGFLOAT_MIN;
  }
  else if (choice == "direct" && indexPath.section == 0)
  {
    return CGFLOAT_MIN;
  }
  
  return [super tableView:tableView heightForRowAtIndexPath:indexPath];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 2)
  {
    // TODO
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

@end
