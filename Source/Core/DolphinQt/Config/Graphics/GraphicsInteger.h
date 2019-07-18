// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QSpinBox>

namespace Config
{
template <typename T>
struct ConfigInfo;
}

class GraphicsInteger : public QSpinBox
{
  Q_OBJECT
public:
  GraphicsInteger(int minimum, int maximum, const Config::ConfigInfo<int>& setting, int step = 1);
  void Update(int value);

private:
  const Config::ConfigInfo<int>& m_setting;
};
