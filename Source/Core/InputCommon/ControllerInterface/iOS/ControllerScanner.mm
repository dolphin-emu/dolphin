// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <GameController/GameController.h>

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/iOS/ControllerScanner.h"
#include "InputCommon/ControllerInterface/iOS/iOS.h"

@implementation ControllerScanner

- (id)init
{
  if (self = [super init])
  {
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(ControllerConnected:)
                                                 name:GCControllerDidConnectNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(ControllerDisconnected:)
                                                 name:GCControllerDidDisconnectNotification
                                               object:nil];
  }

  return self;
}

- (void)BeginControllerScan
{
  [GCController startWirelessControllerDiscoveryWithCompletionHandler:nil];
}

- (void)ControllerConnected:(NSNotification*)notification
{
  // Get the GCController instance
  GCController* controller = (GCController*)notification.object;
  g_controller_interface.AddDevice(std::make_shared<ciface::iOS::Controller>(controller));
}

- (void)ControllerDisconnected:(NSNotification*)notification
{
  // Get the GCController instance
  GCController* gc_controller = (GCController*)notification.object;
  g_controller_interface.RemoveDevice([&gc_controller](const auto* device) {
    const ciface::iOS::Controller* controller =
        dynamic_cast<const ciface::iOS::Controller*>(device);
    return controller && controller->IsSameController(gc_controller);
  });
}

@end