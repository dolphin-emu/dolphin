// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __OBJC__
#include <CoreHaptics/CoreHaptics.h>
#else
struct NSObject;
struct CHHapticEngine;
struct CHHapticAdvancedPatternPlayer;
#endif

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Touch/ButtonManager.h"

namespace ciface::iOS
{
class Motor : public Core::Device::Output
{
public:
  Motor(const std::string name);
  Motor(const std::string name, CHHapticEngine* engine);
  ~Motor();
  std::string GetName() const override;
  void SetState(ControlState state) override;

private:
  void CreatePlayer();
  
  CHHapticEngine* m_haptic_engine;
#ifdef __OBJC__
  id<CHHapticAdvancedPatternPlayer> m_haptic_player;
#else
  // TODO: is this correct?
  CHHapticAdvancedPatternPlayer* m_haptic_player;
#endif
  const std::string m_name;
#ifdef __OBJC__
  id<NSObject> m_notification_token;
#else
  NSObject* m_notification_token;
#endif
  ControlState m_last_state;
};
}  // namespace ciface::iOS
