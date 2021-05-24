// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/GameController/GameControllerDevice.h"

#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"

namespace ciface::GameController
{
using MathUtil::GRAVITY_ACCELERATION; // 9.80665

Controller::Controller(const GCController* controller) : m_controller(controller)
{
  std::vector<std::pair<std::string, GCControllerButtonInput*>> pairs;
    
  for (GCControllerButtonInput* item in controller.physicalInputProfile.allButtons)
  {
    pairs.push_back(std::make_pair(std::string([item.aliases.allObjects[0] UTF8String]), item));
  }
    
  sort(pairs.begin(), pairs.end(), [=](std::pair<std::string, GCControllerButtonInput*>& a, std::pair<std::string, GCControllerButtonInput*>& b)
  {
    return a.first.compare(b.first)<0;
  }
  );
    
  for (size_t i = 0; i < pairs.size(); ++i) {
    INFO_LOG_FMT(SERIALINTERFACE, "Found button '{}'", pairs[i].first);
    AddInput(new Button(pairs[i].second));
  }
    
  if (controller.motion)
  {
    INFO_LOG_FMT(SERIALINTERFACE, "Adding accelerometer inputs");

    // This mapping was only tested with a DualShock 4 controller
    AddInput(new Motion(controller, "Accel Up", Motion::accel_z, -GRAVITY_ACCELERATION));
    AddInput(new Motion(controller, "Accel Down", Motion::accel_z, GRAVITY_ACCELERATION));
    AddInput(new Motion(controller, "Accel Left", Motion::accel_x, -GRAVITY_ACCELERATION));
    AddInput(new Motion(controller, "Accel Right", Motion::accel_x, GRAVITY_ACCELERATION));
    AddInput(new Motion(controller, "Accel Forward", Motion::accel_y, -GRAVITY_ACCELERATION));
    AddInput(new Motion(controller, "Accel Backward", Motion::accel_y, GRAVITY_ACCELERATION));

    if (controller.motion.hasRotationRate) {
      INFO_LOG_FMT(SERIALINTERFACE, "Adding gyro inputs");
            
      AddInput(new Motion(controller, "Gyro Pitch Up", Motion::gyro_x, 1));
      AddInput(new Motion(controller, "Gyro Pitch Down", Motion::gyro_x, -1));
      AddInput(new Motion(controller, "Gyro Roll Left", Motion::gyro_y, 1));
      AddInput(new Motion(controller, "Gyro Roll Right", Motion::gyro_y, -1));
      AddInput(new Motion(controller, "Gyro Yaw Left", Motion::gyro_z, 1));
      AddInput(new Motion(controller, "Gyro Yaw Right", Motion::gyro_z, -1));
    }
 
  }
}

std::string Controller::GetSource() const
{
  return "GCF";
}

std::string Controller::GetName() const
{
  return std::string([m_controller.vendorName UTF8String]);
}

ControlState Controller::Button::GetState() const
{
  return m_item.value;
}

ControlState Controller::Motion::GetState() const
{
  double r = 0.0;

  switch (m_button)
  {
  case accel_x:
    r = m_item.motion.acceleration.x;
    break;
  case accel_y:
    r = m_item.motion.acceleration.y;
    break;
  case accel_z:
    r = m_item.motion.acceleration.z;
    break;
  case gyro_x:
    r = m_item.motion.rotationRate.x;
    break;
  case gyro_y:
    r = m_item.motion.rotationRate.y;
    break;
  case gyro_z:
    r = m_item.motion.rotationRate.z;
    break;
  }
    
  return r * m_multiplier;
}

bool Controller::IsSameDevice(const GCController* other_device) const
{
  return m_controller == other_device;
}

} // namespace ciface::GameController
