// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QAbstractItemModel>
#include <QAbstractItemView>

#include "DolphinQt/QtUtils/ViewScrollLock.h"

ViewScrollLock::ViewScrollLock(QObject* parent, QAbstractItemModel* model, QAbstractItemView* view)
    : QObject(parent), m_view(view)
{
  connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this,
          &ViewScrollLock::AboutToBeModified);
  connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this,
          &ViewScrollLock::AboutToBeModified);
  connect(model, &QAbstractItemModel::rowsInserted, this, &ViewScrollLock::Modified);
  connect(model, &QAbstractItemModel::rowsRemoved, this, &ViewScrollLock::Modified);
}

void ViewScrollLock::AboutToBeModified()
{
  QSize size = m_view->size();
  m_first = m_view->indexAt(QPoint(0, 0));
  m_last = m_view->indexAt(QPoint(size.height(), size.width()));
}

void ViewScrollLock::Modified()
{
  // Try to keep the first row at the top.
  // If that fails, try to keep the last row at the bottom.
  if (m_first.isValid())
    m_view->scrollTo(m_first, QAbstractItemView::PositionAtTop);
  else if (m_last.isValid())
    m_view->scrollTo(m_last, QAbstractItemView::PositionAtBottom);
}
