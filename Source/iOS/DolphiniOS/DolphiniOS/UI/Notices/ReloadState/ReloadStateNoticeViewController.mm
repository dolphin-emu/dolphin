// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "Common/FileUtil.h"

#import "ReloadStateNoticeViewController.h"

@interface ReloadStateNoticeViewController ()

@end

@implementation ReloadStateNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  NSString* game_name = [[NSUserDefaults standardUserDefaults] stringForKey:@"last_game_title"];
  NSString* message = [NSString stringWithFormat:@"You were playing \"%@\" before DolphiniOS was quit.", game_name];
  [self.m_game_name_label setText:message];
}

- (IBAction)ReturnPressed:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
  
  UIStoryboard* storyboard = [UIStoryboard storyboardWithName:@"Emulation" bundle:nil];
  UIViewController* root_controller = [storyboard instantiateViewControllerWithIdentifier:@"Root"];
  [[[[UIApplication sharedApplication] keyWindow] rootViewController] presentViewController:root_controller animated:true completion:nil];
}

- (IBAction)DontReturnPressed:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
  
  File::Delete(File::GetUserPath(D_STATESAVES_IDX) + "backgroundAuto.sav");
}

@end
