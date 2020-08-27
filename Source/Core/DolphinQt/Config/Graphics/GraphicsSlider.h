// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QSlider>

namespace Config
{
template <typename T>
struct Info;
}

class GraphicsSlider : public QSlider
{
  Q_OBJECT
public:
  GraphicsSlider(int minimum, int maximum, const Config::Info<int>& setting, int tick = 0);
  void Update(int value);

private:
  const Config::Info<int>& m_setting;
};
