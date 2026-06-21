// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on the Qt "Flow Layout" example.

#include "DolphinQt/QtUtils/FlowLayout.h"

#include <QWidget>

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
  m_items.append(item);
}

int FlowLayout::horizontalSpacing() const
{
  if (m_h_space >= 0)
    return m_h_space;
  return SmartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const
{
  if (m_v_space >= 0)
    return m_v_space;
  return SmartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

int FlowLayout::count() const
{
  return m_items.size();
}

QLayoutItem* FlowLayout::itemAt(int index) const
{
  return m_items.value(index);
}

QLayoutItem* FlowLayout::takeAt(int index)
{
  if (index >= 0 && index < m_items.size())
    return m_items.takeAt(index);
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
  return DoLayout(QRect(0, 0, width, 0), true);
}

void FlowLayout::setGeometry(const QRect& rect)
{
  QLayout::setGeometry(rect);
  DoLayout(rect, false);
}

QSize FlowLayout::sizeHint() const
{
  // Prefer a single row so the window opens wide enough to show every section; it wraps once the
  // user shrinks it below this width.
  int width = 0;
  int height = 0;
  bool first = true;
  for (const QLayoutItem* item : m_items)
  {
    if (item->isEmpty())
      continue;
    const QSize hint = item->sizeHint();
    if (!first)
      width += horizontalSpacing();
    width += hint.width();
    height = qMax(height, hint.height());
    first = false;
  }

  const QMargins margins = contentsMargins();
  return QSize(width + margins.left() + margins.right(),
               height + margins.top() + margins.bottom());
}

QSize FlowLayout::minimumSize() const
{
  QSize size;
  for (const QLayoutItem* item : m_items)
  {
    if (item->isEmpty())
      continue;
    size = size.expandedTo(item->minimumSize());
  }

  const QMargins margins = contentsMargins();
  size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
  return size;
}

int FlowLayout::DoLayout(const QRect& rect, bool test_only) const
{
  int left, top, right, bottom;
  getContentsMargins(&left, &top, &right, &bottom);
  const QRect effective = rect.adjusted(left, top, -right, -bottom);
  int x = effective.x();
  int y = effective.y();
  int line_height = 0;

  for (QLayoutItem* item : m_items)
  {
    // Hidden sections (toggled off via the options menu) shouldn't reserve space.
    if (item->isEmpty())
      continue;

    const QSize hint = item->sizeHint();
    int space_x = horizontalSpacing();
    int space_y = verticalSpacing();

    int next_x = x + hint.width() + space_x;
    if (next_x - space_x > effective.right() && line_height > 0)
    {
      x = effective.x();
      y = y + line_height + space_y;
      next_x = x + hint.width() + space_x;
      line_height = 0;
    }

    if (!test_only)
      item->setGeometry(QRect(QPoint(x, y), hint));

    x = next_x;
    line_height = qMax(line_height, hint.height());
  }

  return y + line_height - rect.y() + bottom;
}

int FlowLayout::SmartSpacing(QStyle::PixelMetric pm) const
{
  QObject* parent = this->parent();
  if (!parent)
    return -1;

  if (parent->isWidgetType())
  {
    auto* pw = static_cast<QWidget*>(parent);
    return pw->style()->pixelMetric(pm, nullptr, pw);
  }

  return static_cast<QLayout*>(parent)->spacing();
}
