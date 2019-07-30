// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/ModifySettingsButton.h"

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
ModifySettingsButton::ModifySettingsButton(std::string button_name)
    : Buttons(std::move(button_name))
{
}

void ModifySettingsButton::AddInput(std::string button_name, bool toggle)
{
  controls.emplace_back(std::make_unique<Input>(Translate, std::move(button_name)));
  threshold_exceeded.emplace_back(false);
  associated_settings.emplace_back(false);
  associated_settings_toggle.emplace_back(toggle);
}

void ModifySettingsButton::GetState()
{
  for (size_t i = 0; i < controls.size(); ++i)
  {
    ControlState state = controls[i]->control_ref->State();

    if (!associated_settings_toggle[i])
    {
      // not toggled
      associated_settings[i] = state > ACTIVATION_THRESHOLD;
    }
    else
    {
      // toggle (loading savestates does not en-/disable toggle)
      // after we passed the threshold, we en-/disable. but after that, we don't change it
      // anymore
      if (!threshold_exceeded[i] && state > ACTIVATION_THRESHOLD)
      {
        associated_settings[i] = !associated_settings[i];

        if (associated_settings[i])
          OSD::AddMessage(controls[i]->ui_name + ": on");
        else
          OSD::AddMessage(controls[i]->ui_name + ": off");

        threshold_exceeded[i] = true;
      }

      if (state < ACTIVATION_THRESHOLD)
        threshold_exceeded[i] = false;
    }
  }
}

const std::vector<bool>& ModifySettingsButton::isSettingToggled() const
{
  return associated_settings_toggle;
}

const std::vector<bool>& ModifySettingsButton::getSettingsModifier() const
{
  return associated_settings;
}
}  // namespace ControllerEmu
