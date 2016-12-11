// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinUIKit/ViewController.h"
#include "Common/MsgHandler.h"
#include "Core/BootManager.h"
#include "DolphinUIKit/Host.h"

static bool MsgAlert(const char* caption, const char* text, bool yes_no, int Style)
{
  NSLog(@"%s: %s", caption, text);
  return false;
}

@implementation ViewController

- (instancetype)initWithURL:(NSURL*)URL
{
  if (self = [super init])
  {
    _URL = [URL copy];
  }
  return self;
}

- (void)loadView
{
  [super loadView];
  self.view.backgroundColor = [UIColor whiteColor];

  _EAGLLayer = [CAEAGLLayer layer];
  _EAGLLayer.opaque = YES;
  _EAGLLayer.contentsScale = [[UIScreen mainScreen] scale];
  [self.view.layer addSublayer:_EAGLLayer];
}

- (void)dealloc
{
  [_URL release];
  [_EAGLLayer release];

  [super dealloc];
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];

  RegisterMsgAlertHandler(&MsgAlert);
  Host_SetRenderHandle(reinterpret_cast<void*>(_EAGLLayer));

  std::string path = _URL.fileSystemRepresentation;
  if (!BootManager::BootCore(path))
  {
    NSLog(@"Failed to boot!");
    return;
  }

  NSLog(@"Booted.");
}

- (void)viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];

  BootManager::Stop();
  Host_SetRenderHandle(nullptr);
}

- (void)viewWillLayoutSubviews
{
  [super viewWillLayoutSubviews];

  _EAGLLayer.frame = self.view.layer.bounds;
}

@end
