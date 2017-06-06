// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QEvent>
#include <QObject>

#include "DolphinQt2/QtUtils/FocusEventFilter.h"

bool FocusEventFilter::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::FocusOut)
    emit focusOutEvent();

  if (event->type() == QEvent::FocusIn)
    emit focusInEvent();

  return false;
}
