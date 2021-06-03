// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <objc/message.h>

#include "InputCommon/ControllerInterface/GameController/GameController.h"
#include "InputCommon/ControllerInterface/GameController/GameControllerDevice.h"

#include "Common/Logging/Log.h"

namespace ciface::GameController
{
static void AddController(GCController* controller)
{
#if defined(__MAC_11_0)
  if (@available(macOS 11.0, *))
  {
    if (controller.motion.sensorsRequireManualActivation)
    {
      controller.motion.sensorsActive = true;
    }

    std::string vendor_name = std::string([controller.vendorName UTF8String]);
    INFO_LOG_FMT(CONTROLLERINTERFACE, "Controller '{}' connected", vendor_name);

    g_controller_interface.AddDevice(std::make_shared<Controller>(controller));
  }
#else
  INFO_LOG_FMT(CONTROLLERINTERFACE, "macOS 11.0 or later required for motion sensors");
#endif
}

static void OnControllerConnect(CFNotificationCenterRef center, void* observer, CFStringRef name,
                                const void* object, CFDictionaryRef userInfo)
{
  auto* controller = static_cast<GCController*>(object);
  AddController(controller);
}

static void OnControllerDisconnect(CFNotificationCenterRef center, void* observer, CFStringRef name,
                                   const void* object, CFDictionaryRef userInfo)
{
  auto* in_controller = static_cast<GCController*>(object);
  std::string vendor_name = std::string([in_controller.vendorName UTF8String]);
  INFO_LOG_FMT(CONTROLLERINTERFACE, "Controller '{}' disconnected", vendor_name);

  g_controller_interface.RemoveDevice([&in_controller](const auto* obj) {
    const Controller* controller = dynamic_cast<const Controller*>(obj);
    if (controller && controller->IsSameDevice(in_controller))
      return true;

    return false;
  });
}

void Init()
{
  // PopulateDevices() will make sure the initial list of already connected controllers are added.
  // The callbacks below ensure that controllers that are added or removed later are also taken into
  // account.
  CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), nullptr,
                                  &OnControllerConnect, CFSTR("GCControllerDidConnectNotification"),
                                  nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
  CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), nullptr,
                                  &OnControllerDisconnect,
                                  CFSTR("GCControllerDidDisconnectNotification"), nullptr,
                                  CFNotificationSuspensionBehaviorDeliverImmediately);
}

void PopulateDevices()
{
  g_controller_interface.RemoveDevice(
      [](const auto* dev) { return dev->GetSource() == GCF_SOURCE_NAME; });

  // I was unable to use [GCController controllers] in Objective-C++ and therefore needed to access
  // this class method using objc_msgSend instead
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
  CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), nullptr,
                                     CFSTR("GCControllerDidConnectNotification"), nullptr);
  CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), nullptr,
                                     CFSTR("GCControllerDidDisconnectNotification"), nullptr);
}

}  // namespace ciface::GameController
