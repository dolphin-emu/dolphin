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
  void SelectGeneralPane();
  void SelectAudioPane();

private:
  ListTabWidget* m_tabs;
  int m_audio_pane_index = -1;
  int m_general_pane_index = -1;
};
