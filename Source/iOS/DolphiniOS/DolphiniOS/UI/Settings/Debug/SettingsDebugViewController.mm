// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SettingsDebugViewController.h"

#import "Common/Config/Config.h"
#import "Common/FileUtil.h"

#import "Core/Config/MainSettings.h"
#import "Core/ConfigManager.h"

#import "DebuggerUtils.h"

#import "FastmemUtil.h"

#define MISC_ROW_SYNC_ON_SKIP_IDLE 0
#define MISC_ROW_FASTMEM 1
#define MISC_ROW_FASTMEM_HACKY 2
#define MISC_ROW_CONSOLE_LOG 3

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
  [self.m_fastmem_switch setOn:config.bFastmem];
  [self.m_hacky_fastmem_switch setOn:Config::Get(Config::MAIN_DEBUG_HACKY_FASTMEM)];
  
#ifdef NONJAILBROKEN
  bool can_enable_fastmem = CanEnableFastmem();
  [self.m_fastmem_switch setEnabled:can_enable_fastmem];
  [self.m_hacky_fastmem_switch setEnabled:can_enable_fastmem];
#endif
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

- (IBAction)FastmemChanged:(id)sender
{
  SConfig::GetInstance().bFastmem = [self.m_fastmem_switch isOn];
}

- (IBAction)HackyFastmemChanged:(id)sender
{
  Config::SetBaseOrCurrent(Config::MAIN_DEBUG_HACKY_FASTMEM, [self.m_hacky_fastmem_switch isOn]);
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
  else if (indexPath.section == 1)
  {
    if (indexPath.row == MISC_ROW_CONSOLE_LOG)
    {
      NSString* source_path = [[NSBundle mainBundle] pathForResource:@"DebugLogger" ofType:@"ini"];
      std::string dest_path = File::GetUserPath(F_LOGGERCONFIG_IDX);
      
      File::Delete(dest_path);
      File::Copy(FoundationToCppString(source_path), dest_path);
      
      // Reload
      SConfig::GetInstance().LoadSettings();
    }
  }
  
  [self.tableView deselectRowAtIndexPath:indexPath animated:true];
}

- (void)tableView:(UITableView*)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath*)indexPath
{
  if (indexPath.section == 1)
  {
    NSString* message = DOLocalizedString(@"Error");
    if (indexPath.row == MISC_ROW_SYNC_ON_SKIP_IDLE)
    {
      message = NSLocalizedString(@"This setting changes whether the GPU is synchronized with the CPU when the CPU is idle.\n\nWARNING: Disabling this option may result in better performance (FPS), but you may also experience FIFO errors, \"Invalid Opcode\" messages, game glitches, and Dolphin crashes.\n\nIf unsure, leave this setting checked.", nil);
    }
    else if (indexPath.row == MISC_ROW_FASTMEM)
    {
      message = NSLocalizedString(@"This setting changes whether Dolphin uses the faster method of emulating the GameCube / Wii RAM. Non-jailbroken devices cannot enable this option due to iOS limitations.\n\nWARNING: Disabling this option will decrease performance (FPS).", nil);
    }
    else if (indexPath.row == MISC_ROW_FASTMEM_HACKY)
    {
      message = NSLocalizedString(@"This setting changes whether Dolphin uses the 'hacky' version of fastmem. This type of fastmem works on non-jailbroken devices with at least 4GB of RAM for free, but will cause issues in some games. When launching an incompatible game, Dolphin will automatically turn off fastmem altogether to avoid a crash.\n\nWARNING: Disabling this option on a device that doesn't support proper fastmem will cause Dolphin to crash.", nil);
    }
    else if (indexPath.row == MISC_ROW_CONSOLE_LOG)
    {
      message = NSLocalizedString(@"This button will enable debug logging to the console. To undo, delete User/Config/Logger.ini.\n\nWARNING: Enabling this will cause a slight performance decrease.", nil);
    }
    
    UIAlertController* controller = [UIAlertController alertControllerWithTitle:DOLocalizedString(@"Help") message:message preferredStyle:UIAlertControllerStyleAlert];
    
    [controller addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:nil]];
    
    [self presentViewController:controller animated:true completion:nil];
  }
}

- (CGFloat)tableView:(UITableView*)tableView heightForRowAtIndexPath:(NSIndexPath*)indexPath
{
  return [super tableView:tableView heightForRowAtIndexPath:indexPath];
}

@end
