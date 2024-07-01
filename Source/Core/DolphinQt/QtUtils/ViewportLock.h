// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QPersistentModelIndex>

class QAbstractItemModel;
class QAbstractItemView;

// Try to keep visible items in view while items are added/removed.
class ViewportLock : public QObject
{
  Q_OBJECT

public:
  ViewportLock(QObject* parent, QAbstractItemModel* model, QAbstractItemView* view);
  void AboutToBeModified();
  void Modified();

private:
  QAbstractItemView* m_view;
  QPersistentModelIndex m_first, m_last;
};
