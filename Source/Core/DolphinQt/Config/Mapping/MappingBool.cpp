// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingBool.h"

#include "DolphinQt/Config/Mapping/MappingWidget.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

MappingBool::MappingBool(MappingWidget* widget, ControllerEmu::BooleanSetting* setting)
    : QCheckBox(tr(setting->m_ui_name.c_str())), m_parent(widget), m_setting(setting)
{
  Update();
  Connect();
}

void MappingBool::Connect()
{
  connect(this, &QCheckBox::stateChanged, this, [this](int value) {
    m_setting->SetValue(value);
    m_parent->SaveSettings();
    m_parent->GetController()->UpdateReferences(g_controller_interface);
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
