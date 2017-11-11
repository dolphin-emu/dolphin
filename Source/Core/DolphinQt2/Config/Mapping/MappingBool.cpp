// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Mapping/MappingBool.h"

#include "DolphinQt2/Config/Mapping/MappingWidget.h"
#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"

MappingBool::MappingBool(MappingWidget* widget, ControllerEmu::BooleanSetting* setting)
    : QCheckBox(QString::fromStdString(setting->m_ui_name)), m_parent(widget), m_setting(setting)
{
  Update();
  Connect();
}

void MappingBool::Connect()
{
  connect(this, &QCheckBox::stateChanged, this, [this](int value) {
    m_setting->SetValue(value);
    m_parent->SaveSettings();
  });
}

void MappingBool::Clear()
{
  m_setting->SetValue(false);
  m_parent->SaveSettings();
  Update();
}

void MappingBool::Update()
{
  setChecked(m_setting->GetValue());
}
