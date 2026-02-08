// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QTabWidget;

enum class TabIndex
{
  General = 0,
  Audio = 2
};

class SettingsWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit SettingsWindow(QWidget* parent = nullptr);
  void SelectGeneralPane();
  void SelectAudioPane();

private:
  QTabWidget* m_tab_widget;
};
