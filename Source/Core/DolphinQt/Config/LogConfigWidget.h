// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>

class QCheckBox;
class QCloseEvent;
class QListWidget;
class QPushButton;
class QRadioButton;
class QVBoxLayout;

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

  QRadioButton* m_verbosity_notice;
  QRadioButton* m_verbosity_error;
  QRadioButton* m_verbosity_warning;
  QRadioButton* m_verbosity_info;
  QRadioButton* m_verbosity_debug;

  QCheckBox* m_out_file;
  QCheckBox* m_out_console;
  QCheckBox* m_out_window;
  QPushButton* m_types_toggle;
  QListWidget* m_types_list;

  bool m_all_enabled = true;
  bool m_block_save = false;
};
