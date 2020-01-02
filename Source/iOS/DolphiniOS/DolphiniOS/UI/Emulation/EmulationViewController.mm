// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "EmulationViewController.h"

#import "Core/ConfigManager.h"

#import "MainiOS.h"

@interface EmulationViewController ()

@end

@implementation EmulationViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
  
  // Setup renderer view
  if (SConfig::GetInstance().m_strVideoBackend == "Vulkan")
  {
    self.m_renderer_view = self.m_metal_view;
  }
  else
  {
    self.m_renderer_view = self.m_eagl_view;
  }
  
  [self.m_renderer_view setHidden:false];
  
  // Hide navigation bar
  [self.navigationController setNavigationBarHidden:true animated:true];
  
  [NSThread detachNewThreadSelector:@selector(StartEmulation) toTarget:self withObject:nil];
}

- (void)StartEmulation
{
  [MainiOS startEmulationWithFile:[NSString stringWithUTF8String:self.m_game_file->GetFilePath().c_str()] viewController:self view:self.m_renderer_view];
  
  dispatch_sync(dispatch_get_main_queue(), ^{
    [self performSegueWithIdentifier:@"toSoftwareTable" sender:nil];
  });
}

- (IBAction)topEdgeRecognized:(id)sender
{
  // Don't do anything if already visible
  if (![self.navigationController isNavigationBarHidden])
  {
    return;
  }
  
  [self UpdateNavigationBar:false];
  
  // Automatic hide after 5 seconds
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
    [self UpdateNavigationBar:true];
  });
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures
{
  return UIRectEdgeTop;
}

- (bool)prefersStatusBarHidden
{
  return [self.navigationController isNavigationBarHidden];
}

- (void)UpdateNavigationBar:(bool)hidden
{
  [self.navigationController setNavigationBarHidden:hidden animated:true];
  [self setNeedsStatusBarAppearanceUpdate];
}

@end
