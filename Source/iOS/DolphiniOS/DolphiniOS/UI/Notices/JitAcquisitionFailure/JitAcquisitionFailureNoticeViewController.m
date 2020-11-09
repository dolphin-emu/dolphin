// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "JitAcquisitionFailureNoticeViewController.h"

#import "JitAcquisitionUtils.h"

@interface JitAcquisitionFailureNoticeViewController ()

@end

@implementation JitAcquisitionFailureNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  
  NSString* error_message;
  switch (GetJitAcqusitionError())
  {
    case DOLJitErrorNotArm64e:
      error_message = @"Non-jailbroken devices with A11 processors and lower on iOS 14.2 are not supported at this time. If you wish to run DolphiniOS, your device must be jailbroken.";
      break;
    case DOLJitErrorImproperlySigned:
      error_message = @"DolphiniOS is not signed correctly. Please use a known working signing service like AltStore or iOS App Signer.";
      break;
    case DOLJitErrorNeedUpdate:
      error_message = @"Non-jailbroken devices running iOS 14.0 or iOS 14.1 are not supported. If your device has an A12 processor or higher, please update to iOS 14.2. If your device has an A11 processor or lower, it must be jailbroken to run DolphiniOS at this time.";
      break;
    case DOLJitErrorGestaltFailed:
      error_message = @"Failed to query MobileGestalt.";
      break;
    case DOLJitErrorJailbreakdFailed:
      error_message = @"Failed to contact jailbreakd.";
      break;
    case DOLJitErrorCsdbgdFailed:
      error_message = @"Failed to contact csdbgd.";
      break;
    case DOLJitErrorNone:
    default:
      error_message = @"An unknown error has occurred.";
      break;
  }
  
  [self.m_error_label setText:NSLocalizedString(error_message, nil)];
  
  char* specific_message = GetJitAcquisitionErrorMessage();
  if (strlen(specific_message) > 0)
  {
    [self.m_specific_label setText:CToFoundationString(specific_message)];
    [self.m_specific_label setHidden:false];
  }
}

@end
