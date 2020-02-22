// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "CpuEmulationEngineViewController.h"

#import "Core/Config/MainSettings.h"
#import "Core/ConfigManager.h"
#import "Core/PowerPC/PowerPC.h"

@interface CpuEmulationEngineViewController ()

@end

@implementation CpuEmulationEngineViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:[self CpuCoreToRow:SConfig::GetInstance().cpu_core] inSection:0]];
  [cell setAccessoryType:UITableViewCellAccessoryCheckmark];
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath*)indexPath
{
  if ([self CpuCoreToRow:SConfig::GetInstance().cpu_core] != indexPath.row)
  {
    UITableViewCell* cell = [tableView cellForRowAtIndexPath:[NSIndexPath indexPathForRow:[self CpuCoreToRow:SConfig::GetInstance().cpu_core] inSection:0]];
    [cell setAccessoryType:UITableViewCellAccessoryNone];
    
    UITableViewCell* new_cell = [tableView cellForRowAtIndexPath:indexPath];
    [new_cell setAccessoryType:UITableViewCellAccessoryCheckmark];
    
    PowerPC::CPUCore core = [self RowToCpuCore:indexPath.row];
    SConfig::GetInstance().cpu_core = core;
    Config::SetBaseOrCurrent(Config::MAIN_CPU_CORE, core);
    
    [[NSUserDefaults standardUserDefaults] setBool:true forKey:@"did_deliberately_change_cpu_core"];
  }
  
  [tableView deselectRowAtIndexPath:indexPath animated:true];
}

- (int)CpuCoreToRow:(PowerPC::CPUCore)core
{
  switch (core)
  {
    case PowerPC::CPUCore::Interpreter:
      return 0;
    case PowerPC::CPUCore::CachedInterpreter:
      return 1;
    case PowerPC::CPUCore::JITARM64:
      return 2;
    default:
      return -1;
  }
}

- (PowerPC::CPUCore)RowToCpuCore:(NSInteger)row
{
  switch (row)
  {
    case 0:
      return PowerPC::CPUCore::Interpreter;
    case 1:
      return PowerPC::CPUCore::CachedInterpreter;
    case 2:
      return PowerPC::CPUCore::JITARM64;
    default:
      return PowerPC::CPUCore::Interpreter;
  }
}

@end
