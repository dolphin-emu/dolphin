#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackMisc.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "Common/Common.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace ControllerEmu
{
PrimeHackMisc::PrimeHackMisc(const std::string& ini_name, const std::string& group_name)
    : ControlGroup(ini_name, group_name, GroupType::PrimeHack)
{
  
}

PrimeHackMisc::PrimeHackMisc(const std::string& name_) : PrimeHackMisc(name_, name_)
{
}

void PrimeHackMisc::AddInput(std::string button_name, bool toggle)
{
  // controls.emplace_back(std::make_unique<Input>(Translate, std::move(button_name)));
  // threshold_exceeded.emplace_back(false);
  // associated_settings.emplace_back(false);
  // associated_settings_toggle.emplace_back(toggle);
}
}
