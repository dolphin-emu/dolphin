/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** SPDX-License-Identifier: BSD-3-Clause
**
****************************************************************************/

#include "DolphinQt/QtUtils/FlowLayout.h"

#include <QtWidgets>

FlowLayout::FlowLayout(QWidget* parent, int margin, int h_spacing, int v_spacing)
    : QLayout(parent), m_h_space(h_spacing), m_v_space(v_spacing)
{
  setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(int margin, int h_spacing, int v_spacing)
    : m_h_space(h_spacing), m_v_space(v_spacing)
{
  setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
  while (QLayoutItem* item = takeAt(0))
    delete item;
}

void FlowLayout::addItem(QLayoutItem* item)
{
  m_item_list.append(item);
}

int FlowLayout::horizontalSpacing() const
{
  if (m_h_space >= 0)
  {
    return m_h_space;
  }
  else
  {
    return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
  }
}

int FlowLayout::verticalSpacing() const
{
  if (m_v_space >= 0)
  {
    return m_v_space;
  }
  else
  {
    return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
  }
}

int FlowLayout::count() const
{
  return m_item_list.size();
}

QLayoutItem* FlowLayout::itemAt(int index) const
{
  return m_item_list.value(index);
}

QLayoutItem* FlowLayout::takeAt(int index)
{
  if (index >= 0 && index < m_item_list.size())
    return m_item_list.takeAt(index);
  else
    return nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
  return {};
}

bool FlowLayout::hasHeightForWidth() const
{
  return true;
}

int FlowLayout::heightForWidth(int width) const
{
  int height = doLayout(QRect(0, 0, width, 0), true);
  return height;
}

void FlowLayout::setGeometry(const QRect& rect)
{
  QLayout::setGeometry(rect);
  doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const
{
  return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
  QSize size;
  for (const auto& item : m_item_list)
    size = size.expandedTo(item->minimumSize());

  // Any direction's margin works, as they all set the same within the constructor.
  int margin = 0;
  getContentsMargins(&margin, nullptr, nullptr, nullptr);

  size += QSize(2 * margin, 2 * margin);
  return size;
}

int FlowLayout::doLayout(const QRect& rect, bool testOnly) const
{
  int left, top, right, bottom;
  getContentsMargins(&left, &top, &right, &bottom);
  QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
  int x = effectiveRect.x();
  int y = effectiveRect.y();
  int lineHeight = 0;

  for (const auto& item : m_item_list)
  {
    QWidget* wid = item->widget();
    int spaceX = horizontalSpacing();
    if (spaceX == -1)
      spaceX = wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton,
                                           Qt::Horizontal);
    int spaceY = verticalSpacing();
    if (spaceY == -1)
      spaceY = wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton,
                                           Qt::Vertical);
    int nextX = x + item->sizeHint().width() + spaceX;
    if (nextX - spaceX > effectiveRect.right() && lineHeight > 0)
    {
      x = effectiveRect.x();
      y = y + lineHeight + spaceY;
      nextX = x + item->sizeHint().width() + spaceX;
      lineHeight = 0;
    }

    if (!testOnly)
      item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));

    x = nextX;
    lineHeight = qMax(lineHeight, item->sizeHint().height());
  }
  return y + lineHeight - rect.y() + bottom;
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
  QObject* parent = this->parent();
  if (!parent)
  {
    return -1;
  }
  else if (parent->isWidgetType())
  {
    QWidget* pw = static_cast<QWidget*>(parent);
    return pw->style()->pixelMetric(pm, nullptr, pw);
  }
  else
  {
    return static_cast<QLayout*>(parent)->spacing();
  }
}
