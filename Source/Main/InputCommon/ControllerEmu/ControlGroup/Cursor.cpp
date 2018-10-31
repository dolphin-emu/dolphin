// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Cursor::Cursor(const std::string& name_) : ControlGroup(name_, GroupType::Cursor)
{
  for (auto& named_direction : named_directions)
    controls.emplace_back(std::make_unique<Input>(Translate, named_direction));

  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Forward")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Backward")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Hide")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Recenter")));

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Center"), 0.5));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Width"), 0.5));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Height"), 0.5));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 20));
  boolean_settings.emplace_back(std::make_unique<BooleanSetting>(_trans("Relative Input"), false));
  boolean_settings.emplace_back(std::make_unique<BooleanSetting>(_trans("Auto-Hide"), false));
}

Cursor::StateData Cursor::GetState(const bool adjusted)
{
  const ControlState zz = controls[4]->control_ref->State() - controls[5]->control_ref->State();

  // silly being here
  if (zz > m_state.z)
    m_state.z = std::min(m_state.z + 0.1, zz);
  else if (zz < m_state.z)
    m_state.z = std::max(m_state.z - 0.1, zz);

  StateData result;
  result.z = m_state.z;

  if (m_autohide_timer > -1)
  {
    --m_autohide_timer;
  }

  ControlState yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
  ControlState xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

  const ControlState deadzone = numeric_settings[3]->GetValue();

  // reset auto-hide timer
  if (std::abs(m_prev_xx - xx) > deadzone || std::abs(m_prev_yy - yy) > deadzone)
  {
    m_autohide_timer = TIMER_VALUE;
  }

  // hide
  const bool autohide = boolean_settings[1]->GetValue() && m_autohide_timer < 0;
  if (controls[6]->control_ref->State() > 0.5 || autohide)
  {
    result.x = 10000;
    result.y = 0;
  }
  else
  {
    // adjust cursor according to settings
    if (adjusted)
    {
      xx *= (numeric_settings[1]->GetValue() * 2);
      yy *= (numeric_settings[2]->GetValue() * 2);
      yy += (numeric_settings[0]->GetValue() - 0.5);
    }

    // relative input
    if (boolean_settings[0]->GetValue())
    {
      // deadzone to avoid the cursor slowly drifting
      if (std::abs(xx) > deadzone)
        m_state.x = MathUtil::Clamp(m_state.x + xx * SPEED_MULTIPLIER, -1.0, 1.0);
      if (std::abs(yy) > deadzone)
        m_state.y = MathUtil::Clamp(m_state.y + yy * SPEED_MULTIPLIER, -1.0, 1.0);

      // recenter
      if (controls[7]->control_ref->State() > 0.5)
      {
        m_state.x = 0.0;
        m_state.y = 0.0;
      }
    }
    else
    {
      m_state.x = xx;
      m_state.y = yy;
    }

    result.x = m_state.x;
    result.y = m_state.y;
  }

  m_prev_xx = xx;
  m_prev_yy = yy;

  return result;
}
}  // namespace ControllerEmu
