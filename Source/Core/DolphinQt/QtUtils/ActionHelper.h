// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QAction>
#include <QKeySequence>
#include <QString>

// Since we have to support Qt < 5.6, we need our own implementation of addAction(QString&
// text,QObject*,PointerToMemberFunction);
template <typename ParentClass, typename RecieverClass, typename Func>
QAction* AddAction(ParentClass* parent, const QString& text, const RecieverClass* receiver,
                   Func slot, const QKeySequence& shortcut = 0)
{
  QAction* action = parent->addAction(text);
  action->setShortcut(shortcut);
  action->connect(action, &QAction::triggered, receiver, slot);

  return action;
}
