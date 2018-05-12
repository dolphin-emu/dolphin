// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QRadioButton>

#include "Common/Config/Config.h"

class GraphicsRadioInt : public QRadioButton
{
  Q_OBJECT
public:
  GraphicsRadioInt(const QString& label, const Config::ConfigInfo<int>& setting, int value);

private:
  void Update();

  Config::ConfigInfo<int> m_setting;
  int m_value;
};
