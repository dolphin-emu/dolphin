// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class GraphicsWidget : public QWidget
{
  Q_OBJECT
protected:
  virtual void LoadSettings() = 0;
  virtual void SaveSettings() = 0;
};
