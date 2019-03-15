// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingNumeric.h"

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

MappingNumeric::MappingNumeric(MappingWidget* parent, ControllerEmu::NumericSetting* setting)
    : m_setting(*setting)
{
  setRange(setting->m_low, setting->m_high);

  connect(this, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          [this, parent](int value) {
            m_setting.SetValue(static_cast<double>(value) / 100);
            parent->SaveSettings();
          });

  connect(parent, &MappingWidget::ConfigChanged, this, &MappingNumeric::ConfigChanged);
}

void MappingNumeric::ConfigChanged()
{
  const bool old_state = blockSignals(true);
  setValue(m_setting.GetValue() * 100);
  blockSignals(old_state);
}
