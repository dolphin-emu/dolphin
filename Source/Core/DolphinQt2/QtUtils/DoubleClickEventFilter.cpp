// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wobjectimpl.h>
#include <QEvent>

#include "DolphinQt2/QtUtils/DoubleClickEventFilter.h"

W_OBJECT_IMPL(DoubleClickEventFilter)

bool DoubleClickEventFilter::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::MouseButtonDblClick)
    emit doubleClicked();

  return false;
}
