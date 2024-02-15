// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
  ControlGroup::AddInput(Translatability::Translate, std::move(button_name));
  m_threshold_exceeded.emplace_back(false);
  m_associated_settings.emplace_back(false);
  m_associated_settings_toggle.emplace_back(toggle);
}

void ModifySettingsButton::UpdateState()
{
  for (size_t i = 0; i < controls.size(); ++i)
  {
    const bool state = controls[i]->GetState<bool>();

    if (!m_associated_settings_toggle[i])
    {
      // not toggled
      m_associated_settings[i] = state;
    }
    else
    {
      // toggle (loading savestates does not en-/disable toggle)
      // after we passed the threshold, we en-/disable. but after that, we don't change it
      // anymore
      if (!m_threshold_exceeded[i] && state)
      {
        m_associated_settings[i] = !m_associated_settings[i];

        if (m_associated_settings[i])
          OSD::AddMessage(controls[i]->ui_name + ": on");
        else
          OSD::AddMessage(controls[i]->ui_name + ": off");

        m_threshold_exceeded[i] = true;
      }

      if (!state)
        m_threshold_exceeded[i] = false;
    }
  }
}

const std::vector<bool>& ModifySettingsButton::IsSettingToggled() const
{
  return m_associated_settings_toggle;
}

const std::vector<bool>& ModifySettingsButton::GetSettingsModifier() const
{
  return m_associated_settings;
}
}  // namespace ControllerEmu
