// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QSpinBox>
#include <QString>

class MappingWidget;

namespace ControllerEmu
{
class NumericSetting;
}

class MappingNumeric : public QSpinBox
{
public:
  MappingNumeric(MappingWidget* widget, ControllerEmu::NumericSetting* ref);

  void Clear();
  void Update();

private:
  void Connect();

  MappingWidget* m_parent;
  ControllerEmu::NumericSetting* m_setting;
  double m_range;
};
