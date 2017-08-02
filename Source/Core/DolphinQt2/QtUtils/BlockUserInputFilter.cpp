// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/QtUtils/BlockUserInputFilter.h"

#include <QEvent>

BlockUserInputFilter* BlockUserInputFilter::Instance()
{
  static BlockUserInputFilter s_block_user_input_filter;
  return &s_block_user_input_filter;
}

bool BlockUserInputFilter::eventFilter(QObject* object, QEvent* event)
{
  const QEvent::Type event_type = event->type();
  return event_type == QEvent::KeyPress || event_type == QEvent::KeyRelease ||
         event_type == QEvent::MouseButtonPress || event_type == QEvent::MouseButtonRelease ||
         event_type == QEvent::MouseButtonDblClick;
}
