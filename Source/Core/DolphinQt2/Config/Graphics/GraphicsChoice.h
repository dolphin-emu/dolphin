// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QComboBox>

#include "Common/Config/Config.h"

class GraphicsChoice : public QComboBox
{
  Q_OBJECT
public:
  GraphicsChoice(const QStringList& options, const Config::ConfigInfo<int>& setting);

private:
  void Update(int choice);

  Config::ConfigInfo<int> m_setting;
};
