// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Config/Mapping/MappingBool.h"

#include "DolphinQt2/Config/Mapping/MappingWidget.h"
#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"

MappingBool::MappingBool(MappingWidget* widget, ControllerEmu::BooleanSetting* setting)
    : QCheckBox(QString::fromStdString(setting->m_ui_name)), m_parent(widget), m_setting(setting)
{
  setChecked(setting->GetValue());
  Connect();
}

void MappingBool::Connect()
{
  connect(this, &QCheckBox::stateChanged, this, [this](int value) {
    m_setting->SetValue(value);
    Update();
  });
}

void MappingBool::Clear()
{
  setChecked(false);
  Update();
}

void MappingBool::Update()
{
  setChecked(m_setting->GetValue());
  m_parent->SaveSettings();
}
