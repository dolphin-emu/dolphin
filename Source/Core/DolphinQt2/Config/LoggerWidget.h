// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>

#include <mutex>
#include <queue>

#include "Common/Logging/LogManager.h"

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QListWidget;
class QPushButton;
class QRadioButton;
class QVBoxLayout;
class QTabWidget;
class QTextEdit;
class QTimer;

class LoggerWidget final : public QDockWidget, LogListener
{
  Q_OBJECT
public:
  explicit LoggerWidget(QWidget* parent = nullptr);
  ~LoggerWidget();

  bool eventFilter(QObject* object, QEvent* event) override;

private:
  void UpdateLog();
  void UpdateFont();
  void CreateWidgets();
  void ConnectWidgets();
  void CreateMainLayout();
  void LoadSettings();
  void SaveSettings();

  void OnTabVisibilityChanged();

  void Log(LogTypes::LOG_LEVELS level, const char* text) override;

  // Log
  QCheckBox* m_log_wrap;
  QComboBox* m_log_font;
  QPushButton* m_log_clear;
  QVBoxLayout* m_main_layout;
  QTabWidget* m_tab_widget;
  QTextEdit* m_log_text;
  QWidget* m_tab_log;

  // Configuration
  QWidget* m_tab_config;
  QRadioButton* m_verbosity_notice;
  QRadioButton* m_verbosity_error;
  QRadioButton* m_verbosity_warning;
  QRadioButton* m_verbosity_info;
  QCheckBox* m_out_file;
  QCheckBox* m_out_console;
  QCheckBox* m_out_window;
  QPushButton* m_types_toggle;
  QListWidget* m_types_list;

  QTimer* m_timer;

  std::mutex m_log_mutex;
  std::queue<QString> m_log_queue;

  bool m_all_enabled = true;
  bool m_block_save = false;
};
