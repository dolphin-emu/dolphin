// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>

class QSocketNotifier;

// Loosely based on https://doc.qt.io/qt-5.9/unix-signals.html
class SignalDaemon : public QObject
{
  Q_OBJECT

public:
  explicit SignalDaemon(QObject* parent);
  ~SignalDaemon();

  static void HandleInterrupt(int);

signals:
  void InterruptReceived();

private:
  void OnNotifierActivated();

  static int s_sigterm_fd[2];

  QSocketNotifier* m_term;
};
