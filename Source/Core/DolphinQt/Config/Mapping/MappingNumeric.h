// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QString>

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

class MappingWidget;

class MappingDouble : public QDoubleSpinBox
{
public:
  MappingDouble(QWidget* parent, ControllerEmu::NumericSetting<double>* setting,
                bool is_in_mapping_widget);
  void ConfigChanged();

private:
  void Update();
  void fixup(QString& input) const override;

  ControllerEmu::NumericSetting<double>& m_setting;
};

class MappingBool : public QCheckBox
{
public:
  MappingBool(MappingWidget* widget, ControllerEmu::NumericSetting<bool>* setting,
              bool is_in_mapping_widget);
  void ConfigChanged();

private:
  void Update();

  ControllerEmu::NumericSetting<bool>& m_setting;
};
