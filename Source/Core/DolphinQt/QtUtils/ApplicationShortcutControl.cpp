// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/ApplicationShortcutControl.h"

#include <QEvent>
#include <QKeySequence>
#include <QObject>
#include <QVariant>

ApplicationShortcutControl* ApplicationShortcutControl::Instance()
{
  static ApplicationShortcutControl* s_instance = new ApplicationShortcutControl();
  return s_instance;
}

void ApplicationShortcutControl::AddAction(QAction* action)
{
  action->setData(action->shortcut());
  m_actions.append(action);
}

void ApplicationShortcutControl::EnableShortcuts(bool enable_toggle)
{
  if (enable_toggle)
  {
    for (QAction* action : m_actions)
      action->setShortcut(action->data().value<QKeySequence>());
  }
  else
  {
    for (QAction* action : m_actions)
      action->setShortcut(QKeySequence());
  }
}
