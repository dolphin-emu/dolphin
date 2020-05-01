// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "WiiSystemUpdateViewController.h"

#include "Common/FileUtil.h"
#include "Common/Flag.h"

#include "Core/Core.h"
#include "Core/WiiUtils.h"

#include "DiscIO/NANDImporter.h"

@interface WiiSystemUpdateViewController ()

@end

@implementation WiiSystemUpdateViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  // Disable auto-sleep
  [[UIApplication sharedApplication] setIdleTimerDisabled:true];
}

- (void)viewDidAppear:(BOOL)animated
{
  WiiSystemUpdateViewController* this_controller = self;
  WiiUtils::UpdateCallback callback = [this_controller](size_t processed, size_t total, u64 title_id)
  {
    dispatch_sync(dispatch_get_main_queue(), ^{
      if (this_controller.m_is_cancelled)
      {
        return;
      }
      
      [this_controller.m_progress_bar setProgress:(float)processed / (float)total];
      
      NSString* progress_str = DOLocalizedStringWithArgs(@"Updating title %1...\nThis can take a while.", @"016llx");
      [this_controller.m_progress_label setText:[NSString stringWithFormat:progress_str, title_id]];
    });
    
    return !this_controller.m_is_cancelled;
  };
  
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    WiiUtils::UpdateResult result;
    if (self.m_is_online)
    {
      result = WiiUtils::DoOnlineUpdate(callback, FoundationToCppString(self.m_source));
    }
    else
    {
      result = WiiUtils::DoDiscUpdate(callback, FoundationToCppString(self.m_source));
    }
    
    dispatch_async(dispatch_get_main_queue(), ^{
      [self ShowResult:result];
    });
  });
  
}

- (void)viewWillDisappear:(BOOL)animated
{
  // Re-enable auto-sleep
  [[UIApplication sharedApplication] setIdleTimerDisabled:false];
}

- (IBAction)CancelPressed:(id)sender
{
  self.m_is_cancelled = true;
  
  [self.m_progress_bar setTrackTintColor:[UIColor redColor]];
  [self.m_progress_bar setProgress:1.0f];
  
  [self.m_progress_label setText:DOLocalizedString(@"Finishing the update...\nThis can take a while.")];
  
  [self.m_cancel_button setEnabled:false];
}

- (void)ShowResult:(WiiUtils::UpdateResult)result
{
  switch (result)
  {
  case WiiUtils::UpdateResult::Succeeded:
    [self PopUpAlertWithTitle:@"Update completed"
                      message:@"The emulated Wii console has been updated."];
    DiscIO::NANDImporter().ExtractCertificates(File::GetUserPath(D_WIIROOT_IDX));
      
    break;
  case WiiUtils::UpdateResult::AlreadyUpToDate:
    [self PopUpAlertWithTitle:@"Update completed"
                      message:@"The emulated Wii console is already up-to-date."];
    DiscIO::NANDImporter().ExtractCertificates(File::GetUserPath(D_WIIROOT_IDX));
      
    break;
  case WiiUtils::UpdateResult::ServerFailed:
    [self PopUpAlertWithTitle:@"Update failed"
                      message:@"Could not download update information from Nintendo. "
                               "Please check your Internet connection and try again."];
    break;
  case WiiUtils::UpdateResult::DownloadFailed:
    [self PopUpAlertWithTitle:@"Update failed"
                      message:@"Could not download update files from Nintendo. "
                               "Please check your Internet connection and try again."];
    break;
  case WiiUtils::UpdateResult::ImportFailed:
    [self PopUpAlertWithTitle:@"Update failed"
                      message:@"Could not install an update to the Wii system memory. "
                               "Please refer to logs for more information."];
    break;
  case WiiUtils::UpdateResult::Cancelled:
    [self PopUpAlertWithTitle:@"Update cancelled"
                      message:@"The update has been cancelled. It is strongly recommended to "
                               "finish it in order to avoid inconsistent system software versions."];
    break;
  case WiiUtils::UpdateResult::RegionMismatch:
    [self PopUpAlertWithTitle:@"Update failed"
                      message:@"The game's region does not match your console's. "
                               "To avoid issues with the system menu, it is not possible "
                               "to update the emulated console using this disc."];
    break;
  case WiiUtils::UpdateResult::MissingUpdatePartition:
  case WiiUtils::UpdateResult::DiscReadFailed:
    [self PopUpAlertWithTitle:@"Update failed"
                      message:@"The game disc does not contain any usable "
                               "update information."];
    break;
  }
}

- (void)PopUpAlertWithTitle:(NSString*)title message:(NSString*)message
{
  UIAlertController* alert_controller = [UIAlertController alertControllerWithTitle:DOLocalizedString(title) message:DOLocalizedString(message) preferredStyle:UIAlertControllerStyleAlert];
  [alert_controller addAction:[UIAlertAction actionWithTitle:DOLocalizedString(@"OK") style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
    [self dismissViewControllerAnimated:true completion:nil];
  }]];
   
  [self presentViewController:alert_controller animated:true completion:nil];
}

@end
