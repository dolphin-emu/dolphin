// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "DolphinQt/BBAServer.h"

class QLineEdit;
class QSpinBox;
class QPlainTextEdit;

class BBAServerWindow : public QDialog
{
  Q_OBJECT
public:
  explicit BBAServerWindow(QWidget* parent = nullptr);

private slots:
  void Toggle();
  void LogOutput(const QDateTime& timestamp, const QString& log_line);

private:
  void Update();

  BBAServer m_server;

  QLineEdit* m_host_addr;
  QSpinBox* m_port;
  QPushButton* m_toggle;

  QPlainTextEdit* m_log_output;
};
