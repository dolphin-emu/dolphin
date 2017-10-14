// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QSlider>

namespace Config
{
template <typename T>
struct ConfigInfo;
}

class GraphicsSlider : public QSlider
{
public:
  GraphicsSlider(int minimum, int maximum, const Config::ConfigInfo<int>& setting, int tick = 0);
  void Update(int value);

private:
  const Config::ConfigInfo<int>& m_setting;
};
