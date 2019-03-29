// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/TAS/StickWidget.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>

#include "Common/CommonTypes.h"

StickWidget::StickWidget(QWidget* parent, u16 max_x, u16 max_y)
    : QWidget(parent), m_max_x(max_x), m_max_y(max_y)
{
  setMouseTracking(false);
  setToolTip(tr("Left click to set the stick value.\n"
                "Right click to re-center it."));
}

void StickWidget::SetX(u16 x)
{
  m_x = std::min(m_max_x, x);

  update();
}

void StickWidget::SetY(u16 y)
{
  m_y = std::min(m_max_y, y);

  update();
}

void StickWidget::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  painter.setBrush(Qt::white);
  painter.drawEllipse(0, 0, width() - 1, height() - 1);

  painter.drawLine(0, height() / 2, width(), height() / 2);
  painter.drawLine(width() / 2, 0, width() / 2, height());

  // convert from value space to widget space
  u16 x = (m_x * width()) / m_max_x;
  u16 y = height() - (m_y * height()) / m_max_y;

  painter.drawLine(width() / 2, height() / 2, x, y);

  painter.setBrush(Qt::blue);
  int wh_avg = (width() + height()) / 2;
  int radius = wh_avg / 30;
  painter.drawEllipse(x - radius, y - radius, radius * 2, radius * 2);
}

void StickWidget::mousePressEvent(QMouseEvent* event)
{
  handleMouseEvent(event);
  m_ignore_movement = event->button() == Qt::RightButton;
}

void StickWidget::mouseMoveEvent(QMouseEvent* event)
{
  if (!m_ignore_movement)
    handleMouseEvent(event);
}

void StickWidget::handleMouseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::RightButton)
  {
    m_x = std::round(m_max_x / 2.);
    m_y = std::round(m_max_y / 2.);
  }
  else
  {
    // convert from widget space to value space
    int new_x = (event->x() * m_max_x) / width();
    int new_y = m_max_y - (event->y() * m_max_y) / height();

    m_x = std::max(0, std::min(static_cast<int>(m_max_x), new_x));
    m_y = std::max(0, std::min(static_cast<int>(m_max_y), new_y));
  }

  emit ChangedX(m_x);
  emit ChangedY(m_y);
  update();
}
