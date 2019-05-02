// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QString>

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

class MappingWidget;

class MappingDouble : public QDoubleSpinBox
{
public:
  MappingDouble(MappingWidget* parent, ControllerEmu::NumericSetting<double>* setting);

private:
  void fixup(QString& input) const override;

  void ConfigChanged();

  ControllerEmu::NumericSetting<double>& m_setting;
};

class MappingBool : public QCheckBox
{
public:
  MappingBool(MappingWidget* widget, ControllerEmu::NumericSetting<bool>* setting);

private:
  void ConfigChanged();

  ControllerEmu::NumericSetting<bool>& m_setting;
};
