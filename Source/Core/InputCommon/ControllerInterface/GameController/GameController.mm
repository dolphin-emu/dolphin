// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <random>

#import <objc/message.h>

#include "InputCommon/ControllerInterface/GameController/GameController.h"
#include "InputCommon/ControllerInterface/GameController/GameControllerDevice.h"

#include "Common/Logging/Log.h"

namespace ciface::GameController
{
static char *s_observer="";

void addController(GCController* controller)
{
  controller.motion.sensorsActive = true;

  std::string vendorName = std::string([controller.vendorName UTF8String]);
  INFO_LOG_FMT(SERIALINTERFACE, "Controller '{}' connected with motion={}", vendorName, controller.motion.sensorsActive);
  NOTICE_LOG_FMT(SERIALINTERFACE, "Controller '{}' connected with motion={} and id=", vendorName, controller.motion.sensorsActive);

  std::shared_ptr<ciface::GameController::Controller> j=std::make_shared<Controller>(controller);
    
  g_controller_interface.AddDevice(j);
}

static void onControllerConnect(CFNotificationCenterRef center, void* observer, CFStringRef name, const void* object, CFDictionaryRef userInfo)
{
  GCController* controller = (GCController*)object;
  addController(controller);
}

static void onControllerDisconnect(CFNotificationCenterRef center, void* observer, CFStringRef name, const void* object, CFDictionaryRef userInfo)
{
  GCController *inController = (GCController*)object;
    
  std::string vendorName = std::string([inController.vendorName UTF8String]);
  INFO_LOG_FMT(SERIALINTERFACE, "Controller '{}' disconnected", vendorName);
    
  g_controller_interface.RemoveDevice([&inController](const auto* obj)
  {
    const Controller* controller = dynamic_cast<const Controller*>(obj);
    if (controller && controller->IsSameDevice(inController))
      return true;

    return false;
  });
}

void Init(void* window)
{
  CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), s_observer, &onControllerConnect, CFSTR("GCControllerDidConnectNotification"), NULL, CFNotificationSuspensionBehaviorDeliverImmediately);

  CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), s_observer, &onControllerDisconnect, CFSTR("GCControllerDidDisconnectNotification"), NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
}

void PopulateDevices(void* window)
{
  // I was unable to use [GCController controllers] in Objective-C++ and therefore needed to access this class method using objc_msgSend instead
  typedef NSArray<GCController*>* (*send_type)(id, SEL);
  send_type func = (send_type)objc_msgSend;
  NSArray<GCController*>* controllers=(NSArray<GCController*>*)func(objc_getClass("GCController"), sel_getUid("controllers"));

  for (GCController* controller in controllers)
  {
    addController(controller);
  }
}

void DeInit()
{
  CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), s_observer, CFSTR("GCControllerDidConnectNotification"), NULL);
  CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), s_observer, CFSTR("GCControllerDidDisconnectNotification"), NULL);
}

} // namespace ciface::GameController
