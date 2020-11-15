// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <Foundation/Foundation.h>

#include <sstream>

#include "Common/Logging/Log.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/iOS/Motor.h"

#define LOG_NSERROR(x, y) ERROR_LOG(SERIALINTERFACE, x, [[y localizedDescription] UTF8String])

namespace ciface::iOS
{
Motor::Motor(const std::string name) API_AVAILABLE(ios(13.0)) : m_name(name)
{
  if (![[CHHapticEngine capabilitiesForHardware] supportsHaptics])
  {
    return;
  }
  
  if (![[NSUserDefaults standardUserDefaults] boolForKey:@"rumble_enabled"])
  {
    return;
  }
  
  NSError* error;
  this->m_haptic_engine = [[CHHapticEngine alloc] initAndReturnError:&error];

  if (error)
  {
    LOG_NSERROR("Failed to create a CHHapticEngine: %s", error);
    this->m_haptic_engine = nil;

    return;
  }

  [this->m_haptic_engine startAndReturnError:&error];

  if (error)
  {
    LOG_NSERROR("Failed to start the CHHapticEngine: %s", error);
    this->m_haptic_engine = nil;

    return;
  }

  CreatePlayer();
}

Motor::Motor(const std::string name, CHHapticEngine* engine) API_AVAILABLE(ios(13.0)) : m_name(name)
{
  this->m_haptic_engine = engine;

  CreatePlayer();
}

Motor::~Motor() API_AVAILABLE(ios(13.0))
{
  if (this->m_haptic_player)
  {
    NSError* error;
    [this->m_haptic_player stopAtTime:CHHapticTimeImmediate error:&error];

    LOG_NSERROR("Failed to stop haptics on Motor destruction: %s", error);
  }

  this->m_haptic_engine = nil;
  this->m_haptic_player = nil;
}

void Motor::CreatePlayer() API_AVAILABLE(ios(13.0))
{ 
  CHHapticEventParameter* intensity_param = [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:1.0f];

  CHHapticEvent* event = [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous parameters:@[
    intensity_param
  ] relativeTime:0.0f duration:1.0f];
  
  NSError* error;
  CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[ event ] parameters:@[ ] error:&error];

  if (error)
  {
    LOG_NSERROR("Failed to create CHHapticPattern: %s", error);

    return;
  }

  this->m_haptic_player = [this->m_haptic_engine createAdvancedPlayerWithPattern:pattern error:&error];

  if (error)
  {
    LOG_NSERROR("Failed to create CHHapticAdvancedPatternPlayer: %s", error);
    this->m_haptic_player = nil;

    return;
  }

  [this->m_haptic_player setLoopEnabled:true];
  [this->m_haptic_player setLoopEnd:0.0f];
}

std::string Motor::GetName() const
{
  return m_name;
}

void Motor::SetState(ControlState state)
{
  if (!this->m_haptic_player)
  {
    return;
  }

  NSError* error;
  if (state > 0)
  {
    [this->m_haptic_player startAtTime:CHHapticTimeImmediate error:&error];
  }
  else
  {
    [this->m_haptic_player stopAtTime:CHHapticTimeImmediate error:&error];
  }

  if (error)
  {
    LOG_NSERROR("Failed to start/stop haptics: %s", error);
  }
}
} // namespace ciface::Android