// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/TAS/IRWidget.h"

#include <algorithm>

#include <QMouseEvent>
#include <QPainter>

#include "Common/CommonTypes.h"

IRWidget::IRWidget(QWidget* parent) : QWidget(parent)
{
  setMouseTracking(false);
}

void IRWidget::SetX(u16 x)
{
  m_x = std::min(ir_max_x, x);

  update();
}

void IRWidget::SetY(u16 y)
{
  m_y = std::min(ir_max_y, y);

  update();
}

void IRWidget::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);

  painter.setBrush(Qt::white);
  painter.drawRect(0, 0, width() - 1, height() - 1);

  painter.drawLine(0, height() / 2, width(), height() / 2);
  painter.drawLine(width() / 2, 0, width() / 2, height());

  // convert from value space to widget space
  u16 x = width() - (m_x * width()) / ir_max_x;
  u16 y = (m_y * height()) / ir_max_y;

  painter.drawLine(width() / 2, height() / 2, x, y);

  painter.setBrush(Qt::blue);
  int wh_avg = (width() + height()) / 2;
  int radius = wh_avg / 30;
  painter.drawEllipse(x - radius, y - radius, radius * 2, radius * 2);
}

void IRWidget::mousePressEvent(QMouseEvent* event)
{
  handleMouseEvent(event);
}

void IRWidget::mouseMoveEvent(QMouseEvent* event)
{
  handleMouseEvent(event);
}

void IRWidget::handleMouseEvent(QMouseEvent* event)
{
  // convert from widget space to value space
  int new_x = ir_max_x - (event->x() * ir_max_x) / width();
  int new_y = (event->y() * ir_max_y) / height();

  m_x = std::max(0, std::min(static_cast<int>(ir_max_x), new_x));
  m_y = std::max(0, std::min(static_cast<int>(ir_max_y), new_y));

  emit ChangedX(m_x);
  emit ChangedY(m_y);
  update();
}
