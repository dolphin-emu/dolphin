// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QFormLayout;

class GraphicsWidget : public QWidget
{
  Q_OBJECT
protected:
  virtual void LoadSettings() = 0;
  virtual void SaveSettings() = 0;

private:
  QFormLayout* m_main_layout;
};
