// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QSpinBox>

namespace Config
{
template <typename T>
struct Info;
}

class GraphicsInteger : public QSpinBox
{
  Q_OBJECT
public:
  GraphicsInteger(int minimum, int maximum, const Config::Info<int>& setting, int step = 1);
  void Update(int value);

private:
  const Config::Info<int>& m_setting;
};
