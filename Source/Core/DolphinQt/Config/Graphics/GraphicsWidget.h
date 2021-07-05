// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class GraphicsWidget : public QWidget
{
  Q_OBJECT
protected:
  virtual void LoadSettings() = 0;
  virtual void SaveSettings() = 0;
};
