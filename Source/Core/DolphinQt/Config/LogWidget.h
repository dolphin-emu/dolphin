// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDockWidget>

#include <mutex>
#include <string>

#include "Common/FixedSizeQueue.h"
#include "Common/Logging/LogManager.h"

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QPlainTextEdit;
class QPushButton;
class QTimer;

class LogWidget final : public QDockWidget, Common::Log::LogListener
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

  void Log(Common::Log::LogLevel level, const char* text) override;

  // Log
  QCheckBox* m_log_wrap;
  QComboBox* m_log_font;
  QPushButton* m_log_clear;
  QPlainTextEdit* m_log_text;

  QTimer* m_timer;

  using LogEntry = std::pair<std::string, Common::Log::LogLevel>;

  // Maximum number of lines to show in log viewer
  static constexpr int MAX_LOG_LINES = 5000;

  std::mutex m_log_mutex;
  Common::FixedSizeQueue<LogEntry, MAX_LOG_LINES> m_log_ring_buffer;
};
