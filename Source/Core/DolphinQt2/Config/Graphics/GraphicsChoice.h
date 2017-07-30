// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QComboBox>

namespace Config
{
template <typename T>
struct ConfigInfo;
}

class GraphicsChoice : public QComboBox
{
public:
  GraphicsChoice(const QStringList& options, const Config::ConfigInfo<int>& setting);

private:
  void Update(int choice);

  const Config::ConfigInfo<int>& m_setting;
};
