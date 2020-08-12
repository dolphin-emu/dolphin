// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/TAS/IRWidget.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>

#include "Common/CommonTypes.h"

constexpr int PADDING = 1;

IRWidget::IRWidget(QWidget* parent, s16 min_x, s16 max_x, s16 min_y, s16 max_y)
    : QWidget(parent), m_min_x(min_x), m_max_x(max_x), m_min_y(min_y), m_max_y(max_y)
{
  setMouseTracking(false);
  setToolTip(tr("Left click to set the IR value.\n"
                "Right click to re-center it."));

  // If the widget gets too small, it will get deformed.
  setMinimumSize(QSize(64, 48));
}

void IRWidget::SetX(s16 x)
{
  m_x = std::min(m_max_x, x);
  m_x = std::max(m_min_x, x);

  update();
}

void IRWidget::SetY(s16 y)
{
  m_y = std::min(m_max_y, y);
  m_y = std::max(m_min_y, y);

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

  s16 span_x = m_max_x - m_min_x;
  s16 span_y = m_max_y - m_min_y;
  // convert from value space to widget space
  u16 x = PADDING + (((m_x - m_min_x) * w) / span_x);
  u16 y = PADDING + (h - ((m_y - m_min_y) * h) / span_y);

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
  s16 span_x = m_max_x - m_min_x;
  s16 span_y = m_max_y - m_min_y;
  if (event->button() == Qt::RightButton)
  {
    m_x = std::round(span_x / 2.) + m_min_x;
    m_y = std::round(span_y / 2.) + m_min_y;
  }
  else
  {
    // convert from widget space to value space.
    s16 new_x = (event->x() * span_x) / width() + m_min_x;
    s16 new_y = span_y - (event->y() * span_y) / height() + m_min_y;

    m_x = std::max(m_min_x, std::min(m_max_x, new_x));
    m_y = std::max(m_min_y, std::min(m_max_y, new_y));
  }

  emit ChangedX(m_x);
  emit ChangedY(m_y);
  update();
}
