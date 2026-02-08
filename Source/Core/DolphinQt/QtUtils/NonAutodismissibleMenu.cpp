// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/NonAutodismissibleMenu.h"

#include <QAction>
#include <QMouseEvent>

namespace QtUtils
{

void NonAutodismissibleMenu::mouseReleaseEvent(QMouseEvent* const event)
{
  if (!event)
    return;

  QAction* const action{activeAction()};
  if (action && action->isEnabled() && action->isCheckable())
  {
    action->trigger();
    event->accept();
    return;
  }

  QMenu::mouseReleaseEvent(event);
}

}  // namespace QtUtils
