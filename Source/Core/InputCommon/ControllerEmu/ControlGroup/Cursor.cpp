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
    controls.emplace_back(std::make_unique<Input>(named_direction));

  controls.emplace_back(std::make_unique<Input>(_trans("Forward")));
  controls.emplace_back(std::make_unique<Input>(_trans("Backward")));
  controls.emplace_back(std::make_unique<Input>(_trans("Hide")));
  controls.emplace_back(std::make_unique<Input>(_trans("Recenter")));

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Center"), 0.5));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Width"), 0.5));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Height"), 0.5));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 20));
  boolean_settings.emplace_back(std::make_unique<BooleanSetting>(_trans("Relative Input"), false));
}

void Cursor::GetState(ControlState* const x, ControlState* const y, ControlState* const z,
                      const bool adjusted)
{
  const ControlState zz = controls[4]->control_ref->State() - controls[5]->control_ref->State();

  // silly being here
  if (zz > m_z)
    m_z = std::min(m_z + 0.1, zz);
  else if (zz < m_z)
    m_z = std::max(m_z - 0.1, zz);

  *z = m_z;

  // hide
  if (controls[6]->control_ref->State() > 0.5)
  {
    *x = 10000;
    *y = 0;
  }
  else
  {
    ControlState yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
    ControlState xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

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
      const ControlState deadzone = numeric_settings[3]->GetValue();
      // deadzone to avoid the cursor slowly drifting
      if (std::abs(xx) > deadzone)
        m_x = MathUtil::Clamp(m_x + xx * SPEED_MULTIPLIER, -1.0, 1.0);
      if (std::abs(yy) > deadzone)
        m_y = MathUtil::Clamp(m_y + yy * SPEED_MULTIPLIER, -1.0, 1.0);

      // recenter
      if (controls[7]->control_ref->State() > 0.5)
      {
        m_x = 0.0;
        m_y = 0.0;
      }
    }
    else
    {
      m_x = xx;
      m_y = yy;
    }

    *x = m_x;
    *y = m_y;
  }
}
}  // namespace ControllerEmu
