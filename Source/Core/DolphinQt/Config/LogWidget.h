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

class LogWidget final : public QDockWidget
{
  Q_OBJECT
public:
  explicit LogWidget(QWidget* parent = nullptr);
  ~LogWidget();

protected:
  void closeEvent(QCloseEvent*) override;

private:
  // LogListener instances are owned by LogManager, so we can't make LogWidget inherit from
  // LogListener, since Qt should be in control of LogWidget's lifetime. Instead we have
  // this LogListenerImpl class to act as an adapter.
  class LogListenerImpl final : public Common::Log::LogListener
  {
  public:
    explicit LogListenerImpl(LogWidget* log_widget) : m_log_widget(log_widget) {}

  private:
    void Log(Common::Log::LogLevel level, const char* text) override
    {
      m_log_widget->Log(level, text);
    }

    LogWidget* m_log_widget;
  };

  void UpdateLog();
  void UpdateFont();
  void CreateWidgets();
  void ConnectWidgets();
  void LoadSettings();
  void SaveSettings();

  void Log(Common::Log::LogLevel level, const char* text);

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
