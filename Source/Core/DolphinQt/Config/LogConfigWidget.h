// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>

class QCheckBox;
class QCloseEvent;
class QTreeView;

class LogConfigWidget final : public QDockWidget
{
  Q_OBJECT
public:
  explicit LogConfigWidget(QWidget* parent = nullptr);
  ~LogConfigWidget();

protected:
  void closeEvent(QCloseEvent* event) override;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void LoadSettings();
  void SaveSettings();

  QCheckBox* m_out_file;
  QCheckBox* m_out_console;
  QCheckBox* m_out_window;
  QTreeView* m_log_types;
};
