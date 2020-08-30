// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwarePropertiesViewController.h"

#import "SoftwareActionReplayViewController.h"
#import "SoftwareGeckoViewController.h"
#import "SoftwareInfoViewController.h"
#import "SoftwarePatchesViewController.h"
#import "SoftwareVerifyStartViewController.h"

@interface SoftwarePropertiesViewController ()

@end

@implementation SoftwarePropertiesViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  self.navigationItem.title = CppToFoundationString(self.m_game_file->GetUniqueIdentifier());
}

- (IBAction)DonePressed:(id)sender
{
  [self.navigationController dismissViewControllerAnimated:true completion:nil];
}

- (void)prepareForSegue:(UIStoryboardSegue*)segue sender:(id)sender
{
  if ([segue.identifier isEqualToString:@"to_info"])
  {
    ((SoftwareInfoViewController*)(segue.destinationViewController)).m_game_file = self.m_game_file;
  }
  else if ([segue.identifier isEqualToString:@"to_verify"])
  {
    ((SoftwareVerifyStartViewController*)(segue.destinationViewController)).m_game_file = self.m_game_file;
  }
  else if ([segue.identifier isEqualToString:@"to_gecko"])
  {
    ((SoftwareGeckoViewController*)segue.destinationViewController).m_game_file = self.m_game_file;
  }
  else if ([segue.identifier isEqualToString:@"to_patches"])
  {
    ((SoftwarePatchesViewController*)segue.destinationViewController).m_game_file = self.m_game_file;
  }
  else if ([segue.identifier isEqualToString:@"to_ar"])
  {
    ((SoftwareActionReplayViewController*)segue.destinationViewController).m_game_file = self.m_game_file;
  }
}

- (IBAction)unwindToSoftwareProperties:(UIStoryboardSegue*)segue {}

@end
