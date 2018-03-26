// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QRadioButton>

namespace Config
{
template <typename T>
struct ConfigInfo;
}

class GraphicsRadioInt : public QRadioButton
{
  Q_OBJECT
public:
  GraphicsRadioInt(const QString& label, const Config::ConfigInfo<int>& setting, int value);

private:
  void Update();

  const Config::ConfigInfo<int>& m_setting;
  int m_value;
};
