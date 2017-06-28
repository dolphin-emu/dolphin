// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wobjectimpl.h>
#include <QEvent>
#include <QObject>

#include "DolphinQt2/QtUtils/WindowActivationEventFilter.h"

W_OBJECT_IMPL(WindowActivationEventFilter)

bool WindowActivationEventFilter::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::WindowDeactivate)
    emit windowDeactivated();

  if (event->type() == QEvent::WindowActivate)
    emit windowActivated();

  return false;
}
