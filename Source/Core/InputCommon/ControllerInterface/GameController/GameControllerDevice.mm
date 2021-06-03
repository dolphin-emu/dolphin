// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/GameController/GameControllerDevice.h"

#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"

namespace ciface::GameController
{
constexpr std::string_view GCF_UNKNOWN_PREFIX = "Unknown ";

Controller::Controller(const GCController* controller) : m_controller(controller)
{
  std::vector<std::pair<std::string, GCControllerButtonInput*>> pairs;
  using MathUtil::GRAVITY_ACCELERATION;
  int unknown_index = 1;

#if defined(__MAC_11_0)
  if (@available(macOS 11.0, *))
  {
    for (GCControllerButtonInput* item in controller.physicalInputProfile.allButtons)
    {
      if (item.aliases.count > 0)
      {
        pairs.push_back(std::make_pair(std::string([item.aliases.allObjects[0] UTF8String]), item));
      }
      else
      {
        pairs.push_back(std::make_pair(
            std::string(GCF_UNKNOWN_PREFIX) + std::to_string(unknown_index++), item));
      }
    }

    std::sort(pairs.begin(), pairs.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.first.compare(rhs.first) < 0; });

    for (size_t i = 0; i < pairs.size(); ++i)
    {
      INFO_LOG_FMT(CONTROLLERINTERFACE, "Found button '{}'", pairs[i].first);
      AddInput(new Button(pairs[i].second, pairs[i].first));
    }

    if (!controller.motion)
      return;

    INFO_LOG_FMT(CONTROLLERINTERFACE, "Adding accelerometer inputs");

    // This mapping was only tested with a DualShock 4 controller
    AddInput(new Motion(controller, "Accel Up", Motion::AccelZ, -GRAVITY_ACCELERATION));
    AddInput(new Motion(controller, "Accel Down", Motion::AccelZ, GRAVITY_ACCELERATION));
    AddInput(new Motion(controller, "Accel Left", Motion::AccelX, -GRAVITY_ACCELERATION));
    AddInput(new Motion(controller, "Accel Right", Motion::AccelX, GRAVITY_ACCELERATION));
    AddInput(new Motion(controller, "Accel Forward", Motion::AccelY, -GRAVITY_ACCELERATION));
    AddInput(new Motion(controller, "Accel Backward", Motion::AccelY, GRAVITY_ACCELERATION));

    if (!controller.motion.hasRotationRate)
      return;

    INFO_LOG_FMT(CONTROLLERINTERFACE, "Adding gyro inputs");

    // This mapping was only tested with a DualShock 4 controller
    AddInput(new Motion(controller, "Gyro Pitch Up", Motion::GyroX, 1));
    AddInput(new Motion(controller, "Gyro Pitch Down", Motion::GyroX, -1));
    AddInput(new Motion(controller, "Gyro Roll Left", Motion::GyroY, 1));
    AddInput(new Motion(controller, "Gyro Roll Right", Motion::GyroY, -1));
    AddInput(new Motion(controller, "Gyro Yaw Left", Motion::GyroZ, 1));
    AddInput(new Motion(controller, "Gyro Yaw Right", Motion::GyroZ, -1));
  }
#endif
}

std::string Controller::GetSource() const
{
  return GCF_SOURCE_NAME;
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
  ControlState r = 0.0;

#if defined(__MAC_11_0)
  if (@available(macOS 11.0, *))
  {
    switch (m_button)
    {
    case AccelX:
      r = m_item.motion.acceleration.x;
      break;
    case AccelY:
      r = m_item.motion.acceleration.y;
      break;
    case AccelZ:
      r = m_item.motion.acceleration.z;
      break;
    case GyroX:
      r = m_item.motion.rotationRate.x;
      break;
    case GyroY:
      r = m_item.motion.rotationRate.y;
      break;
    case GyroZ:
      r = m_item.motion.rotationRate.z;
      break;
    }
  }
#endif

  return r * m_multiplier;
}

bool Controller::IsSameDevice(const GCController* other_device) const
{
  return m_controller == other_device;
}

}  // namespace ciface::GameController
