// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/QtUtils/SignalDaemon.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <QSocketNotifier>

int SignalDaemon::s_sigterm_fd[2];

static constexpr char message[] =
    "\nA signal was received. A second signal will force Dolphin to stop.\n";

SignalDaemon::SignalDaemon(QObject* parent) : QObject(parent)
{
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, s_sigterm_fd))
    qFatal("Couldn't create TERM socketpair");

  m_term = new QSocketNotifier(s_sigterm_fd[1], QSocketNotifier::Read, this);
  connect(m_term, &QSocketNotifier::activated, this, &SignalDaemon::OnNotifierActivated);
}

SignalDaemon::~SignalDaemon()
{
  close(s_sigterm_fd[0]);
  close(s_sigterm_fd[1]);
}

void SignalDaemon::OnNotifierActivated()
{
  m_term->setEnabled(false);

  char tmp;
  if (read(s_sigterm_fd[1], &tmp, sizeof(char)))
  {
  }

  m_term->setEnabled(true);

  emit InterruptReceived();
}

void SignalDaemon::HandleInterrupt(int)
{
  write(STDERR_FILENO, message, sizeof(message));

  char a = 1;
  if (write(s_sigterm_fd[0], &a, sizeof(a)))
  {
  }
}
