// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "Common/FileUtil.h"

#import "ReloadFailedNoticeViewController.h"

@interface ReloadFailedNoticeViewController ()

@end

@implementation ReloadFailedNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  NSDictionary* last_game_data = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"last_game_boot_info"];
  NSString* message = [NSString stringWithFormat:@"You were playing \"%@\" before DolphiniOS was quit.", [last_game_data objectForKey:@"user_facing_name"]];
  [self.m_game_name_label setText:message];
  
  if (self.m_reason == DOLReloadFailedReasonOld)
  {
    [self.m_reason_label setText:@"While DolphiniOS automatically made a save state before closing, this version of DolphiniOS can not load save states from previous versions."];
  }
  else if (self.m_reason == DOLReloadFailedReasonFileGone)
  {
    [self.m_reason_label setText:@"While DolphiniOS automatically made a save state before closing, the game file cannot be found, so the save state cannot be loaded."];
  }
}

- (IBAction)OKPressed:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
  
  File::Delete(File::GetUserPath(D_STATESAVES_IDX) + "backgroundAuto.sav");
}

@end
