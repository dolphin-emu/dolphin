// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AutoStates.h"

#import "Common/FileUtil.h"

#import "EmulationViewController.h"

#import "ReloadStateNoticeViewController.h"

@interface ReloadStateNoticeViewController ()

@end

@implementation ReloadStateNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  NSDictionary* last_game_data = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"last_game_boot_info"];
  DOLAutoStateBootType boot_type = (DOLAutoStateBootType)[last_game_data[@"boot_type"] integerValue];
  if (boot_type == DOLAutoStateBootTypeNand)
  {
    u64 title_id = [last_game_data[@"location"] unsignedLongLongValue];
    self->m_boot_parameters = std::make_unique<BootParameters>(BootParameters::NANDTitle{title_id});
  }
  else if (boot_type == DOLAutoStateBootTypeGCIPL)
  {
    DiscIO::Region region = static_cast<DiscIO::Region>([last_game_data[@"location"] integerValue]);
    self->m_boot_parameters = std::make_unique<BootParameters>(BootParameters::IPL{region});
  }
  else // Game file
  {
    NSString* last_game_path = last_game_data[@"location"];
    self->m_boot_parameters = BootParameters::GenerateFromFile(FoundationToCppString(last_game_path));
  }
  
  self->m_boot_parameters->savestate_path = File::GetUserPath(D_STATESAVES_IDX) + "backgroundAuto.sav";
  self->m_boot_parameters->delete_savestate = true;
  
  NSString* message = [NSString stringWithFormat:@"You were playing \"%@\" before DolphiniOS was quit.", [last_game_data objectForKey:@"user_facing_name"]];
  [self.m_game_name_label setText:message];
}

- (IBAction)ReturnPressed:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
  
  UIStoryboard* storyboard = [UIStoryboard storyboardWithName:@"Emulation" bundle:nil];
  UINavigationController* navigation_controller = [storyboard instantiateViewControllerWithIdentifier:@"Root"];
  EmulationViewController* emulation_controller = [navigation_controller.viewControllers firstObject];
  emulation_controller->m_boot_parameters = std::move(self->m_boot_parameters);
  
  [[[[UIApplication sharedApplication] keyWindow] rootViewController] presentViewController:navigation_controller animated:true completion:nil];
}

- (IBAction)DontReturnPressed:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
  
  File::Delete(File::GetUserPath(D_STATESAVES_IDX) + "backgroundAuto.sav");
}

@end
