// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QEvent>
#include <QObject>

#include "DolphinQt/QtUtils/WindowActivationEventFilter.h"

bool WindowActivationEventFilter::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::WindowDeactivate)
    emit windowDeactivated();

  if (event->type() == QEvent::WindowActivate)
    emit windowActivated();

  return false;
}
