// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerRootViewController.h"

#import <optional>

#import "Core/ConfigManager.h"
#import "Core/HW/GCPad.h"
#import "Core/HW/Wiimote.h"
#import "Core/HW/WiimoteReal/WiimoteReal.h"

#import "ControllerDetailsViewController.h"
#import "ControllerSettingsUtils.h"

#import "InputCommon/InputConfig.h"

@interface ControllerRootViewController ()

@end

@implementation ControllerRootViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  // Localize port labels
  for (size_t i = 0; i < 4; i++)
  {
    [[self.m_port_name_labels objectAtIndex:i] setText:[NSString localizedStringWithFormat:DOLocalizedStringWithArgs(@"Port %1", @"d"), i + 1]];
    [[self.m_wiimote_name_labels objectAtIndex:i] setText:[NSString localizedStringWithFormat:DOLocalizedStringWithArgs(@"Wii Remote %1", @"d"), i + 1]];
  }
  
  Pad::LoadConfig();
  Wiimote::LoadConfig();
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  for (size_t i = 0; i < 4; i++)
  {
    const std::optional<int> gc_index = [ControllerSettingsUtils SIDevicesToGCMenuIndex:SConfig::GetInstance().m_SIDevice[i]];
    if (gc_index)
    {
      [[self.m_port_labels objectAtIndex:i] setText:[ControllerSettingsUtils GetLocalizedGameCubeControllerFromIndex:gc_index.value()]];
    }
  }
  
  for (size_t i = 0; i < 4; i++)
  {
    [[self.m_wiimote_labels objectAtIndex:i] setText:[ControllerSettingsUtils GetLocalizedWiimoteStringFromSource:WiimoteCommon::GetSource(i)]];
  }
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section != 2)
  {
    [self performSegueWithIdentifier:@"to_controller_edit" sender:nil];
  }
}

- (void)prepareForSegue:(UIStoryboardSegue*)segue sender:(id)sender
{
  if ([segue.identifier isEqualToString:@"to_controller_edit"])
  {
    ControllerDetailsViewController* view_controller = (ControllerDetailsViewController*)segue.destinationViewController;
    NSIndexPath* index_path = [self.tableView indexPathForSelectedRow];
    
    InputConfig* input_config;
    if (index_path.section == 0)
    {
      input_config = Pad::GetConfig();
      view_controller.m_is_wii = false;
    }
    else
    {
      input_config = Wiimote::GetConfig();
      view_controller.m_is_wii = true;
    }
    
    view_controller.m_port = (int)index_path.row;
    view_controller.m_controller = input_config->GetController(view_controller.m_port);
  }
}


@end
