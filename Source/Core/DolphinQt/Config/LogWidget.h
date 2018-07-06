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
class QPushButton;
class QVBoxLayout;
class QTextEdit;
class QTimer;

class LogWidget final : public QDockWidget, LogListener
{
  Q_OBJECT
public:
  explicit LogWidget(QWidget* parent = nullptr);
  ~LogWidget();

protected:
  void closeEvent(QCloseEvent*) override;

private:
  void UpdateLog();
  void UpdateFont();
  void CreateWidgets();
  void ConnectWidgets();
  void LoadSettings();
  void SaveSettings();

  void Log(LogTypes::LOG_LEVELS level, const char* text) override;

  // Log
  QCheckBox* m_log_wrap;
  QComboBox* m_log_font;
  QPushButton* m_log_clear;
  QVBoxLayout* m_main_layout;
  QTextEdit* m_log_text;
  QWidget* m_tab_log;

  QTimer* m_timer;

  std::mutex m_log_mutex;
  std::queue<QString> m_log_queue;
};
