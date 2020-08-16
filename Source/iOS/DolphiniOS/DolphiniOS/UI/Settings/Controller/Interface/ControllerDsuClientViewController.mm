// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "ControllerDsuClientViewController.h"

#import "Common/Config/Config.h"

#import "InputCommon/ControllerInterface/DualShockUDPClient/DualShockUDPClient.h"

@interface ControllerDsuClientViewController ()

@end

@implementation ControllerDsuClientViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  bool enabled = Config::Get(ciface::DualShockUDPClient::Settings::SERVERS_ENABLED);
  [self.m_enabled_switch setOn:enabled];
  
  NSString* ip = [NSString stringWithUTF8String:Config::Get(ciface::DualShockUDPClient::Settings::SERVER_ADDRESS).c_str()];
  [self.m_ip_field setText:ip];
  
  NSString* port = [NSString stringWithFormat:@"%d", Config::Get(ciface::DualShockUDPClient::Settings::SERVER_PORT)];
  [self.m_port_field setText:port];
  
  [self.m_ip_field setEnabled:enabled];
  [self.m_port_field setEnabled:enabled];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 1)
  {
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"https://wiki.dolphin-emu.org/index.php?title=DSU_Client"] options:@{} completionHandler:nil];
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

- (IBAction)SwitchedEnabled:(id)sender
{
  bool enabled = self.m_enabled_switch.isOn;
  Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVERS_ENABLED, enabled);
  
  [self.m_ip_field setEnabled:enabled];
  [self.m_port_field setEnabled:enabled];
}

- (IBAction)IpEdited:(id)sender
{
  std::string ip = std::string([self.m_ip_field.text cStringUsingEncoding:NSUTF8StringEncoding]);
  Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVER_ADDRESS, ip);
}

- (IBAction)PortEdited:(id)sender
{
  int value = [self.m_port_field.text intValue];
  Config::SetBaseOrCurrent(ciface::DualShockUDPClient::Settings::SERVER_PORT, value);
  
  NSString* port = [NSString stringWithFormat:@"%d", Config::Get(ciface::DualShockUDPClient::Settings::SERVER_PORT)];
  [self.m_port_field setText:port];
}

- (BOOL)textFieldShouldReturn:(UITextField*)text_field
{
    [text_field resignFirstResponder];
    return true;
}

@end
