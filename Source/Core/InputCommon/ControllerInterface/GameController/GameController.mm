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

static void AddController(GCController* controller)
{
  controller.motion.sensorsActive = true;

  std::string vendor_name = std::string([controller.vendorName UTF8String]);
  INFO_LOG_FMT(SERIALINTERFACE, "Controller '{}' connected with motion={}", vendor_name, controller.motion.sensorsActive);

  g_controller_interface.AddDevice(std::make_shared<Controller>(controller));
}

static void OnControllerConnect(CFNotificationCenterRef center, void* observer, CFStringRef name, const void* object, CFDictionaryRef userInfo)
{
  auto* controller = static_cast<GCController*>(object);
  AddController(controller);
}

static void OnControllerDisconnect(CFNotificationCenterRef center, void* observer, CFStringRef name, const void* object, CFDictionaryRef userInfo)
{
  auto* in_controller = static_cast<GCController*>(object);
    
  std::string vendor_name = std::string([in_controller.vendorName UTF8String]);
  INFO_LOG_FMT(SERIALINTERFACE, "Controller '{}' disconnected", vendor_name);
    
  g_controller_interface.RemoveDevice([&in_controller](const auto* obj)
  {
    const Controller* controller = dynamic_cast<const Controller*>(obj);
    if (controller && controller->IsSameDevice(in_controller))
      return true;

    return false;
  });
}

void Init(void* window)
{
  CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), s_observer, &OnControllerConnect, CFSTR("GCControllerDidConnectNotification"), nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);

  CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), s_observer, &OnControllerDisconnect, CFSTR("GCControllerDidDisconnectNotification"), nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
}

void PopulateDevices(void* window)
{
  // I was unable to use [GCController controllers] in Objective-C++ and therefore needed to access this class method using objc_msgSend instead
  using SendType = NSArray<GCController*>* (*)(id, SEL);
  SendType func = (SendType)objc_msgSend;
  auto* controllers = func(objc_getClass("GCController"), sel_getUid("controllers"));

  for (GCController* controller in controllers)
  {
      AddController(controller);
  }
}

void DeInit()
{
  CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), s_observer, CFSTR("GCControllerDidConnectNotification"), nullptr);
  CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), s_observer, CFSTR("GCControllerDidDisconnectNotification"), nullptr);
}

} // namespace ciface::GameController
