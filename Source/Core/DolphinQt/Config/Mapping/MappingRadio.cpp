// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingRadio.h"

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

MappingRadio::MappingRadio(MappingWidget* parent, ControllerEmu::BooleanSetting* setting)
    : QRadioButton(tr(setting->m_ui_name.c_str())), m_setting(*setting)
{
  connect(this, &QRadioButton::toggled, this, [this, parent](int value) {
    m_setting.SetValue(value);
    parent->SaveSettings();
  });

  connect(parent, &MappingWidget::ConfigChanged, this, &MappingRadio::ConfigChanged);
}

void MappingRadio::ConfigChanged()
{
  const bool old_state = blockSignals(true);
  setChecked(m_setting.GetValue());
  blockSignals(old_state);
}
