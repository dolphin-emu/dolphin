// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Mapping/MappingNumeric.h"

#include "DolphinQt2/Config/Mapping/MappingWidget.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

MappingNumeric::MappingNumeric(MappingWidget* widget, ControllerEmu::NumericSetting* setting)
    : m_parent(widget), m_setting(setting), m_range(setting->m_high - setting->m_low)
{
  setRange(setting->m_low, setting->m_high);
  Update();
  Connect();
}

void MappingNumeric::Connect()
{
  connect(this, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          [this](int value) {
            m_setting->SetValue(static_cast<double>(value - m_setting->m_low) / m_range);
            m_parent->SaveSettings();
          });
}

void MappingNumeric::Clear()
{
  m_setting->SetValue(m_setting->m_low + (m_setting->m_low + m_setting->m_high) / 2);
  m_parent->SaveSettings();
  Update();
}

void MappingNumeric::Update()
{
  setValue(m_setting->m_low + m_setting->GetValue() * m_range);
}
