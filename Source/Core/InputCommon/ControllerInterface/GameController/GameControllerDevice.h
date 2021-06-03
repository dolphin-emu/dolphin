// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <GameController/GameController.h>

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/GameController/GameController.h"

namespace ciface::GameController
{
class Controller : public Core::Device
{
public:
  explicit Controller(const GCController* controller);
  ~Controller() {}

  std::string GetName() const override;
  std::string GetSource() const override;
  bool IsSameDevice(const GCController* controller) const;

private:
  const GCController* m_controller;

  class Button : public Input
  {
  public:
    explicit Button(GCControllerButtonInput* item, std::string name) : m_item(item), m_name(name) {}
    std::string GetName() const { return m_name; }
    ControlState GetState() const override;

  private:
    const GCControllerButtonInput* m_item;
    const std::string m_name;
  };

  class Motion : public Input
  {
  public:
    enum ButtonAxis
    {
      AccelX = 0,
      AccelY,
      AccelZ,
      GyroX,
      GyroY,
      GyroZ
    };

    explicit Motion(const GCController* item, std::string name, ButtonAxis button,
                    double multiplier)
        : m_item(item), m_name(name), m_button(button), m_multiplier(multiplier)
    {
    }
    std::string GetName() const { return m_name; }
    ControlState GetState() const override;

  private:
    const GCController* m_item;
    const std::string m_name;
    const ButtonAxis m_button;
    const double m_multiplier;
  };
};

}  // namespace ciface::GameController
