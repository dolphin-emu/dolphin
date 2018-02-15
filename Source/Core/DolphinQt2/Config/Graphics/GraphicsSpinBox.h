// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QSpinBox>

namespace Config
{
template <typename T>
struct ConfigInfo;
}

class GraphicsSpinBox : public QSpinBox
{
public:
  GraphicsSpinBox(int min, int max, const Config::ConfigInfo<int>& setting);

private:
  void Update(int value);

  const Config::ConfigInfo<int>& m_setting;
};
