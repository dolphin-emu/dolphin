// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "SoftwareVerifyComputeViewController.h"

#import "DiscIO/Volume.h"

#import "SoftwareVerifyResult.h"
#import "SoftwareVerifyResultsViewController.h"

@interface SoftwareVerifyComputeViewController ()

@end

@implementation SoftwareVerifyComputeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  // Do any additional setup after loading the view.
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  // Disable auto-sleep
  [[UIApplication sharedApplication] setIdleTimerDisabled:true];

  self.navigationItem.hidesBackButton = true;
  self.navigationController.interactivePopGestureRecognizer.enabled = false;
  
  if (@available(iOS 13, *))
  {
    self.navigationController.modalInPresentation = true;
    
    // Set gear image
    [self.m_gear_image setImage:[UIImage imageNamed:@"SF_gear"]];
  }
  
  [self.navigationController setNavigationBarHidden:true];
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    std::shared_ptr<DiscIO::Volume> volume = DiscIO::CreateVolume(self.m_game_file->GetFilePath());
    DiscIO::VolumeVerifier verifier(*volume, self.m_verify_redump, {self.m_calc_crc32, self.m_calc_md5, self.m_calc_sha1});
    
    verifier.Start();
    
    u64 total = verifier.GetTotalBytes();
    u64 processed = verifier.GetBytesProcessed();
    while (processed != total && !self.m_is_cancelled)
    {
      dispatch_sync(dispatch_get_main_queue(), ^{
        [self.m_progress_view setProgress:(float)(processed) / (float)total];
      });
      
      verifier.Process();
      
      processed = verifier.GetBytesProcessed();
    }
    
    verifier.Finish();
    
    const DiscIO::VolumeVerifier::Result result = verifier.GetResult();
    SoftwareVerifyResult* objc_result = [[SoftwareVerifyResult alloc] init];
    
    NSMutableArray<SoftwareVerifyProblem*>* objc_problems = [[NSMutableArray alloc] init];
    for (DiscIO::VolumeVerifier::Problem problem : result.problems)
    {
      SoftwareVerifyProblem* objc_problem = [[SoftwareVerifyProblem alloc] init];
      
      NSString* severity;
      switch (problem.severity)
      {
        case DiscIO::VolumeVerifier::Severity::Low:
          severity = DOLocalizedString(@"Low");
          break;
        case DiscIO::VolumeVerifier::Severity::Medium:
          severity = DOLocalizedString(@"Medium");
          break;
        case DiscIO::VolumeVerifier::Severity::High:
          severity = DOLocalizedString(@"High");
          break;
        case DiscIO::VolumeVerifier::Severity::None:
          severity = DOLocalizedString(@"Information");
          break;
      }
      
      objc_problem.m_severity = severity;
      objc_problem.m_description = CppToFoundationString(problem.text);
      
      [objc_problems addObject:objc_problem];
    }
    
    objc_result.m_problems = objc_problems;
    
    void(^set_hash)(NSString*, bool, const std::vector<u8>&) = ^(NSString* property_name, bool has_hash, const std::vector<u8>& hash) {
      NSString* hash_str;
      if (has_hash)
      {
        NSMutableString* hex_str = [[NSMutableString alloc] init];
        for (u8 byte : hash)
        {
          [hex_str appendFormat:@"%02x", byte];
        }
        
        hash_str = [hex_str copy];
      }
      else
      {
        hash_str = DOLocalizedString(@"None");
      }
      
      [objc_result setValue:hash_str forKey:property_name];
    };
    
    set_hash(@"m_crc32", self.m_calc_crc32, result.hashes.crc32);
    set_hash(@"m_md5", self.m_calc_md5, result.hashes.md5);
    set_hash(@"m_sha1", self.m_calc_sha1, result.hashes.sha1);
    
    if (!result.redump.message.empty())
    {
      objc_result.m_redump_status = CppToFoundationString(result.redump.message);
    }
    else
    {
      objc_result.m_redump_status = DOLocalizedString(@"None");
    }
    
    objc_result.m_summary = CppToFoundationString(result.summary_text);
    
    self.m_result = objc_result;
    
    dispatch_async(dispatch_get_main_queue(), ^{
      [self SegueToNextController];
    });
  });
}

- (IBAction)CancelPressed:(id)sender
{
  self.m_is_cancelled = true;
}

- (void)SegueToNextController
{
  if (self.m_is_cancelled)
  {
    [self performSegueWithIdentifier:@"to_verify_start" sender:nil];
  }
  else
  {
    [self performSegueWithIdentifier:@"to_results" sender:nil];
  }
}

- (void)prepareForSegue:(UIStoryboardSegue*)segue sender:(id)sender
{
  if ([segue.identifier isEqualToString:@"to_results"])
  {
    SoftwareVerifyResultsViewController* results_controller = (SoftwareVerifyResultsViewController*)segue.destinationViewController;
    results_controller.m_result = self.m_result;
  }
}

@end
