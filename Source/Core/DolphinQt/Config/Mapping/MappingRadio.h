// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QRadioButton>

class MappingWidget;

namespace ControllerEmu
{
class BooleanSetting;
};

class MappingRadio : public QRadioButton
{
public:
  MappingRadio(MappingWidget* widget, ControllerEmu::BooleanSetting* setting);

  void Clear();
  void Update();

private:
  void Connect();

  MappingWidget* m_parent;
  ControllerEmu::BooleanSetting* m_setting;
};
