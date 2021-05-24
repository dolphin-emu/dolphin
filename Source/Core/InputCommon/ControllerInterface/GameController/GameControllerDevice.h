// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <GameController/GameController.h>

#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::GameController
{

class Controller : public Core::Device
{
private:
  class Button : public Input
  {
  public:
    Button(GCControllerButtonInput* item) : m_item(item) {}
    std::string GetName() const { return std::string([m_item.aliases.allObjects[0] UTF8String]); }
    ControlState GetState() const override;

  private:
    const GCControllerButtonInput* m_item;
  };

  class Motion : public Input
  {
  public:
    enum button
    {
      accel_x = 0,
      accel_y,
      accel_z,
      gyro_x,
      gyro_y,
      gyro_z
    };
      
    Motion(const GCController* item, std::string name, button button, double multiplier) : m_item(item), m_name(name), m_button(button), m_multiplier(multiplier) {}
    std::string GetName() const { return m_name; }
    ControlState GetState() const override;

  private:
    const GCController* m_item;
    const std::string m_name;
    const button m_button;
    const double m_multiplier;
  };

public:
  Controller(const GCController* controller);
  ~Controller() {}

  std::string GetName() const override;
  std::string GetSource() const override;
  bool IsSameDevice(const GCController* controller) const;

private:
  const GCController* m_controller;
};

}  // namespace ciface::GameController
