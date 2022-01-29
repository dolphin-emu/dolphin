// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/DoubleClickEventFilter.h"

#include <QEvent>

bool DoubleClickEventFilter::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::MouseButtonDblClick)
    emit doubleClicked();

  return false;
}
