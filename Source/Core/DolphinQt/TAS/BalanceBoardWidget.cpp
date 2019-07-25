// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/TAS/BalanceBoardWidget.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>

#include "Common/CommonTypes.h"

BalanceBoardWidget::BalanceBoardWidget(QWidget* parent) : QWidget(parent)
{
  setMouseTracking(false);
  setToolTip(tr("Left click to set the IR value.\n"
                "Right click to re-center it."));
}

void BalanceBoardWidget::SetX(u16 x)
{
  m_x = std::min(balance_range, x);

  update();
}

void BalanceBoardWidget::SetY(u16 y)
{
  m_y = std::min(balance_range, y);

  update();
}

void BalanceBoardWidget::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  painter.setBrush(Qt::white);
  painter.drawRect(0, 0, width() - 1, height() - 1);

  painter.drawLine(0, height() / 2, width(), height() / 2);
  painter.drawLine(width() / 2, 0, width() / 2, height());

  // convert from value space to widget space
  const int x = (m_x * width()) / balance_range;
  const int y = height() - (m_y * height()) / balance_range;

  painter.drawLine(width() / 2, height() / 2, x, y);

  painter.setBrush(Qt::blue);
  const int wh_avg = (width() + height()) / 2;
  const int radius = wh_avg / 30;
  painter.drawEllipse(x - radius, y - radius, radius * 2, radius * 2);
}

void BalanceBoardWidget::mousePressEvent(QMouseEvent* event)
{
  handleMouseEvent(event);
  m_ignore_movement = event->button() == Qt::RightButton;
}

void BalanceBoardWidget::mouseMoveEvent(QMouseEvent* event)
{
  if (!m_ignore_movement)
    handleMouseEvent(event);
}

void BalanceBoardWidget::handleMouseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::RightButton)
  {
    m_x = std::round(balance_range / 2.);
    m_y = std::round(balance_range / 2.);
  }
  else
  {
    // convert from widget space to value space
    const int new_x = (event->x() * balance_range) / width();
    const int new_y = balance_range - (event->y() * balance_range) / height();

    m_x = std::max(0, std::min(static_cast<int>(balance_range), new_x));
    m_y = std::max(0, std::min(static_cast<int>(balance_range), new_y));
  }

  emit ChangedX(m_x);
  emit ChangedY(m_y);
  update();
}
