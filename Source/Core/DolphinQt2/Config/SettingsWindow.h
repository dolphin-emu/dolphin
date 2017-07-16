// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class ListTabWidget;

class SettingsWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit SettingsWindow(QWidget* parent = nullptr);
signals:
  void EmulationStarted();
  void EmulationStopped();

private:
  ListTabWidget* m_tabs;
};
