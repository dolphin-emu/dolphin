#pragma once
#pragma once

#include <string>
#include <vector>

#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/Slider.h"

namespace ControllerEmu
{
class PrimeHackMisc : public ControlGroup
{
public:
  explicit PrimeHackMisc(const std::string& name_);
  PrimeHackMisc(const std::string& ini_name, const std::string& group_name);

  void AddInput(std::string button_name, bool toggle = false);

private:
  
};
}  // namespace ControllerEmu
