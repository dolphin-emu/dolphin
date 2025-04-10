// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QAction>
#include <QList>
#include <QObject>

class ApplicationShortcutControl : public QObject
{
  Q_OBJECT
public:
  static ApplicationShortcutControl* Instance();

  void AddAction(QAction* action);
  void EnableShortcuts(bool enable_toggle);

private:
  QList<QAction*> m_actions;
};
