// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "EmulationViewController.h"
#import "DolphiniOS-Swift.h"
#import "EAGLView.h"
#import "MVKView.h"
#import "MainiOS.h"

@interface EmulationViewController ()

@end

@implementation EmulationViewController

- (id)initWithFile:(NSString*)file videoBackend:(NSString*)videoBackend;
{
  self = [self init];
  self.targetFile = file;
  self.videoBackend = videoBackend;

  self.modalPresentationStyle = UIModalPresentationFullScreen;

  return self;
}

- (void)viewDidLoad
{
  [super viewDidLoad];

  dispatch_queue_t queue = dispatch_queue_create("dolphinQueue", NULL);

  UIView* view = [self view];

  dispatch_async(queue, ^{
    [MainiOS startEmulationWithFile:self.targetFile view:view];
  });
}

- (void)loadView
{
  if ([self.videoBackend isEqualToString:@"OGL"])
  {
    self.view = [[EAGLView alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  }
  else
  {
    self.view = [[MVKView alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  }

  self.padView = [[[NSBundle mainBundle] loadNibNamed:@"TCGameCubePad" owner:self
                                              options:nil] objectAtIndex:0];
  self.padView.frame = self.view.bounds;
  [self.view addSubview:self.padView];
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation
{
  return UIInterfaceOrientationLandscapeRight;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
  return UIInterfaceOrientationMaskLandscape;
}

@end
