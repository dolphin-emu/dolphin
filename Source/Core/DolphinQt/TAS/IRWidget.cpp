// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/IRWidget.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>

#include "Common/CommonTypes.h"

constexpr int PADDING = 1;

IRWidget::IRWidget(QWidget* parent) : QWidget(parent)
{
  setMouseTracking(false);
  setToolTip(tr("Left click to set the IR value.\n"
                "Right click to re-center it."));

  // If the widget gets too small, it will get deformed.
  setMinimumSize(QSize(64, 48));
}

void IRWidget::SetX(u16 x)
{
  m_x = std::min(IR_MAX_X, x);

  update();
}

void IRWidget::SetY(u16 y)
{
  m_y = std::min(IR_MAX_Y, y);

  update();
}

void IRWidget::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const int w = width() - PADDING * 2;
  const int h = height() - PADDING * 2;

  painter.setBrush(Qt::white);
  painter.drawRect(PADDING, PADDING, w, h);

  painter.drawLine(PADDING, PADDING + h / 2, PADDING + w, PADDING + h / 2);
  painter.drawLine(PADDING + w / 2, PADDING, PADDING + w / 2, PADDING + h);

  // convert from value space to widget space
  u16 x = PADDING + ((m_x * w) / IR_MAX_X);
  u16 y = PADDING + (h - (m_y * h) / IR_MAX_Y);

  painter.drawLine(PADDING + w / 2, PADDING + h / 2, x, y);

  painter.setBrush(Qt::blue);
  int wh_avg = (w + h) / 2;
  int radius = wh_avg / 30;
  painter.drawEllipse(x - radius, y - radius, radius * 2, radius * 2);
}

void IRWidget::mousePressEvent(QMouseEvent* event)
{
  handleMouseEvent(event);
  m_ignore_movement = event->button() == Qt::RightButton;
}

void IRWidget::mouseMoveEvent(QMouseEvent* event)
{
  if (!m_ignore_movement)
    handleMouseEvent(event);
}

void IRWidget::handleMouseEvent(QMouseEvent* event)
{
  u16 prev_x = m_x;
  u16 prev_y = m_y;

  if (event->button() == Qt::RightButton)
  {
    m_x = std::round(IR_MAX_X / 2.);
    m_y = std::round(IR_MAX_Y / 2.);
  }
  else
  {
    // convert from widget space to value space
    int new_x = (event->pos().x() * IR_MAX_X) / width();
    int new_y = IR_MAX_Y - (event->pos().y() * IR_MAX_Y) / height();

    m_x = std::max(0, std::min(static_cast<int>(IR_MAX_X), new_x));
    m_y = std::max(0, std::min(static_cast<int>(IR_MAX_Y), new_y));
  }

  bool changed = false;
  if (prev_x != m_x)
  {
    emit ChangedX(m_x);
    changed = true;
  }
  if (prev_y != m_y)
  {
    emit ChangedY(m_y);
    changed = true;
  }

  if (changed)
    update();
}
