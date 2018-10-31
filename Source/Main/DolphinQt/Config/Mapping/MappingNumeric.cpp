// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingNumeric.h"

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

MappingNumeric::MappingNumeric(MappingWidget* widget, ControllerEmu::NumericSetting* setting)
    : m_parent(widget), m_setting(setting)
{
  setRange(setting->m_low, setting->m_high);
  Update();
  Connect();
}

void MappingNumeric::Connect()
{
  connect(this, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          [this](int value) {
            m_setting->SetValue(static_cast<double>(value) / 100);
            m_parent->SaveSettings();
            m_parent->GetController()->UpdateReferences(g_controller_interface);
          });
}

void MappingNumeric::Clear()
{
  m_setting->SetValue(m_setting->m_default_value);
  m_parent->SaveSettings();
  Update();
}

void MappingNumeric::Update()
{
  setValue(m_setting->GetValue() * 100);
}
