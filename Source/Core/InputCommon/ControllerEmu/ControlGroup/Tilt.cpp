// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "Common/Common.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Tilt::Tilt(const std::string& name_) : ControlGroup(name_, GroupType::Tilt)
{
  controls.emplace_back(std::make_unique<Input>(_trans("Forward")));
  controls.emplace_back(std::make_unique<Input>(_trans("Backward")));
  controls.emplace_back(std::make_unique<Input>(_trans("Left")));
  controls.emplace_back(std::make_unique<Input>(_trans("Right")));

  controls.emplace_back(std::make_unique<Input>(_trans("Modifier")));

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Circle Stick"), 0));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Angle"), 0.9, 0, 180));
}

void Tilt::GetState(ControlState* const x, ControlState* const y, const bool step)
{
  // this is all a mess

  ControlState yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
  ControlState xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

  ControlState deadzone = numeric_settings[0]->GetValue();
  ControlState circle = numeric_settings[1]->GetValue();
  auto const angle = numeric_settings[2]->GetValue() / 1.8;
  ControlState m = controls[4]->control_ref->State();

  // deadzone / circle stick code
  // this section might be all wrong, but its working good enough, I think

  ControlState ang = atan2(yy, xx);
  ControlState ang_sin = sin(ang);
  ControlState ang_cos = cos(ang);

  // the amt a full square stick would have at current angle
  ControlState square_full =
      std::min(ang_sin ? 1 / fabs(ang_sin) : 2, ang_cos ? 1 / fabs(ang_cos) : 2);

  // the amt a full stick would have that was (user setting circular) at current angle
  // I think this is more like a pointed circle rather than a rounded square like it should be
  ControlState stick_full = (square_full * (1 - circle)) + (circle);

  ControlState dist = sqrt(xx * xx + yy * yy);

  // dead zone code
  dist = std::max(0.0, dist - deadzone * stick_full);
  dist /= (1 - deadzone);

  // circle stick code
  ControlState amt = dist / stick_full;
  dist += (square_full - 1) * amt * circle;

  if (m)
    dist *= 0.5;

  yy = std::max(-1.0, std::min(1.0, ang_sin * dist));
  xx = std::max(-1.0, std::min(1.0, ang_cos * dist));

  // this is kinda silly here
  // gui being open will make this happen 2x as fast, o well

  // silly
  if (step)
  {
    if (xx > m_tilt[0])
      m_tilt[0] = std::min(m_tilt[0] + 0.1, xx);
    else if (xx < m_tilt[0])
      m_tilt[0] = std::max(m_tilt[0] - 0.1, xx);

    if (yy > m_tilt[1])
      m_tilt[1] = std::min(m_tilt[1] + 0.1, yy);
    else if (yy < m_tilt[1])
      m_tilt[1] = std::max(m_tilt[1] - 0.1, yy);
  }

  *y = m_tilt[1] * angle;
  *x = m_tilt[0] * angle;
}
}  // namespace ControllerEmu
