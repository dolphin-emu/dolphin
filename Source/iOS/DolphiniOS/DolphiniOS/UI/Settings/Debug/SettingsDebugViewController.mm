// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SettingsDebugViewController.h"

#import "AppDelegate.h"

#import "Common/CPUDetect.h"

#import "Core/ConfigManager.h"

#import "DebuggerUtils.h"

@interface SettingsDebugViewController ()

@end

@implementation SettingsDebugViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  SConfig& config = SConfig::GetInstance();
  [self.m_load_store_switch setOn:config.bJITLoadStoreOff];
  [self.m_load_store_fp_switch setOn:config.bJITLoadStoreFloatingOff];
  [self.m_load_store_p_switch setOn:config.bJITLoadStorePairedOff];
  [self.m_fp_switch setOn:config.bJITFloatingPointOff];
  [self.m_p_switch setOn:config.bJITPairedOff];
  [self.m_integer_switch setOn:config.bJITIntegerOff];
  [self.m_system_registers_switch setOn:config.bJITSystemRegistersOff];
  [self.m_branch_switch setOn:config.bJITBranchOff];
  [self.m_register_cache_switch setOn:config.bJITRegisterCacheOff];
  
  [self.m_skip_idle_switch setOn:config.bSyncGPUOnSkipIdleHack];
  [self.m_aprr_switch setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"aprr_jit_on"]];
  [self.m_aprr_switch setEnabled:cpu_info.bCanAPRR];
  
  if (IsProcessDebugged())
  {
    [self DisableSetDebuggedCell];
  }
}

- (void)DisableSetDebuggedCell
{
  [self.m_set_debugged_cell setSelectionStyle:UITableViewCellSelectionStyleNone];
  [self.m_set_debugged_label setTextColor:[UIColor grayColor]];
}

- (IBAction)LoadStoreChanged:(id)sender
{
  SConfig::GetInstance().bJITLoadStoreOff = [self.m_load_store_switch isOn];
}

- (IBAction)LoadStoreFpChanged:(id)sender
{
  SConfig::GetInstance().bJITLoadStoreFloatingOff = [self.m_load_store_fp_switch isOn];
}

- (IBAction)LoadStorePChanged:(id)sender
{
  SConfig::GetInstance().bJITLoadStorePairedOff = [self.m_load_store_p_switch isOn];
}

- (IBAction)FpChanged:(id)sender
{
  SConfig::GetInstance().bJITFloatingPointOff = [self.m_fp_switch isOn];
}

- (IBAction)IntegerChanged:(id)sender
{
  SConfig::GetInstance().bJITIntegerOff = [self.m_integer_switch isOn];
}

- (IBAction)PChanged:(id)sender
{
  SConfig::GetInstance().bJITPairedOff = [self.m_p_switch isOn];
}

- (IBAction)SystemRegistersChanged:(id)sender
{
  SConfig::GetInstance().bJITSystemRegistersOff = [self.m_system_registers_switch isOn];
}

- (IBAction)BranchChanged:(id)sender
{
  SConfig::GetInstance().bJITBranchOff = [self.m_branch_switch isOn];
}

- (IBAction)RegisterCacheChanged:(id)sender
{
  SConfig::GetInstance().bJITRegisterCacheOff = [self.m_register_cache_switch isOn];
}

- (IBAction)SkipIdleChanged:(id)sender
{
  SConfig::GetInstance().bSyncGPUOnSkipIdleHack = [self.m_skip_idle_switch isOn];
}

- (IBAction)AprrJitChanged:(id)sender
{
  [[NSUserDefaults standardUserDefaults] setBool:[self.m_aprr_switch isOn] forKey:@"aprr_jit_on"];
  cpu_info.bAPRR = [self.m_aprr_switch isOn];
  
  NSString* message = NSLocalizedString(@"DolphiniOS will quit to apply changes.", nil);
  
  UIAlertController* controller = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Warning") message:message preferredStyle:UIAlertControllerStyleAlert];
  
  [controller addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
    [AppDelegate Shutdown];
    exit(0);
  }]];
  
  [self presentViewController:controller animated:true completion:nil];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  // Jit help button
  if (indexPath.section == 0 && indexPath.row == 0)
  {
    NSString* message = NSLocalizedString(@"These settings will disable parts of the JIT CPU emulator and force Dolphin to use the more accurate interpreter CPU emulator instead.\n\nWARNING: Disabling parts of the JIT could bypass game bugs and emulation errors at the cost of performance (FPS).\n\nIf unsure, leave all settings unchecked.", nil);
    
    UIAlertController* controller = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Help") message:message preferredStyle:UIAlertControllerStyleAlert];
    
    [controller addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
    
    [self presentViewController:controller animated:true completion:nil];
  }
  else if (indexPath.section == 1 && indexPath.row == 2)
  {
    if (!IsProcessDebugged())
    {
      SetProcessDebugged();
      [self DisableSetDebuggedCell];
    }
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

- (void)tableView:(UITableView*)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath*)indexPath
{
  NSString* message;
  if (indexPath.section == 1 && indexPath.row == 0)
  {
    message = NSLocalizedString(@"This setting changes whether the GPU is synchronized with the CPU when the CPU is idle.\n\nWARNING: Disabling this option may result in better performance (FPS), but you may also experience FIFO errors, \"Invalid Opcode\" messages, game glitches, and Dolphin crashes.\n\nIf unsure, leave this setting checked.", nil);
  }
  else if (indexPath.section == 1 && indexPath.row == 1)
  {
    message = NSLocalizedString(@"This setting changes whether the APRR JIT is enabled. It may result in performance increases, but it is highly experimental and can cause crashes. In addition, only devices with A11 processors and higher support APRR.\n\nIf unsure, leave this setting unchecked.", nil);
  }
  else if (indexPath.section == 1 && indexPath.row == 2)
  {
    message = NSLocalizedString(@"This button will set DolphiniOS's process as debugged using a hack.\n\nThis hack can trigger an iOS bug which will cause DolphiniOS to crash or freeze on launch if it is quit by iOS. If you are finished using the emulator, you should press the Quit button in Settings to quit DolphiniOS manually to avoid this bug.\n\nOnly press this button if you are experiencing memory errors or freezes when you start a game.", nil);
  }
  
  UIAlertController* controller = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Help") message:message preferredStyle:UIAlertControllerStyleAlert];
  
  [controller addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
  
  [self presentViewController:controller animated:true completion:nil];
}

- (CGFloat)tableView:(UITableView*)tableView heightForRowAtIndexPath:(NSIndexPath*)indexPath
{
//#ifndef NONJAILBROKEN
  if (indexPath.section == 1 && indexPath.row == 2)
  {
    return CGFLOAT_MIN;
  }
//#endif
  
  return [super tableView:tableView heightForRowAtIndexPath:indexPath];
}

@end
