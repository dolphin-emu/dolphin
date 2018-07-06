// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QCheckBox>
#include <QRadioButton>

namespace Config
{
template <typename T>
struct ConfigInfo;
}

class GraphicsBool : public QCheckBox
{
  Q_OBJECT
public:
  GraphicsBool(const QString& label, const Config::ConfigInfo<bool>& setting, bool reverse = false);

private:
  void Update();

  const Config::ConfigInfo<bool>& m_setting;
  bool m_reverse;
};

class GraphicsBoolEx : public QRadioButton
{
  Q_OBJECT
public:
  GraphicsBoolEx(const QString& label, const Config::ConfigInfo<bool>& setting,
                 bool reverse = false);

private:
  void Update();

  const Config::ConfigInfo<bool>& m_setting;
  bool m_reverse;
};
