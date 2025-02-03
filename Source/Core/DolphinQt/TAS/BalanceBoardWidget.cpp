// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/BalanceBoardWidget.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QPainter>

#include "Common/CommonTypes.h"

BalanceBoardWidget::BalanceBoardWidget(QWidget* parent) : QWidget(parent)
{
  setMouseTracking(false);
  setToolTip(tr("Left click to set the balance value.\n"
                "Right click to return to perfect balance."));
}

void BalanceBoardWidget::SetTR(double top_right)
{
  m_top_right = top_right;
  if (m_reentrance_level < MAX_REENTRANCE)
  {
    const ReentranceBlocker blocker(this);
    emit ChangedTotal(TotalWeight());
  }
  update();
}

void BalanceBoardWidget::SetBR(double bottom_right)
{
  m_bottom_right = bottom_right;
  if (m_reentrance_level < MAX_REENTRANCE)
  {
    const ReentranceBlocker blocker(this);
    emit ChangedTotal(TotalWeight());
  }
  update();
}

void BalanceBoardWidget::SetTL(double top_left)
{
  m_top_left = top_left;
  if (m_reentrance_level < MAX_REENTRANCE)
  {
    const ReentranceBlocker blocker(this);
    emit ChangedTotal(TotalWeight());
  }
  update();
}

void BalanceBoardWidget::SetBL(double bottom_left)
{
  m_bottom_left = bottom_left;
  if (m_reentrance_level < MAX_REENTRANCE)
  {
    const ReentranceBlocker blocker(this);
    emit ChangedTotal(TotalWeight());
  }
  update();
}

void BalanceBoardWidget::SetTotal(double total)
{
  const double current_total = TotalWeight();
  if (current_total != 0)
  {
    const double ratio = total / current_total;
    m_top_right *= ratio;
    m_bottom_right *= ratio;
    m_top_left *= ratio;
    m_bottom_left *= ratio;
  }
  else
  {
    m_top_right = total / 4;
    m_bottom_right = total / 4;
    m_top_left = total / 4;
    m_bottom_left = total / 4;
  }
  if (m_reentrance_level < MAX_REENTRANCE)
  {
    const ReentranceBlocker blocker(this);
    emit ChangedTR(m_top_right);
    emit ChangedBR(m_bottom_right);
    emit ChangedTL(m_top_left);
    emit ChangedBL(m_bottom_left);

    const double new_total = TotalWeight();
    if (new_total != total)
    {
      // This probably shouldn't happen, and I probably should round out numbers a bit closer
      emit ChangedTotal(new_total);
    }
  }
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

  // Compute center of balance
  const double total = TotalWeight();
  const double right = m_top_right + m_bottom_right;
  const double left = m_top_left + m_bottom_left;
  const double top = m_top_right + m_top_left;
  const double bottom = m_bottom_right + m_bottom_left;
  const double com_x = (total != 0) ? (right - left) / total : 0;
  const double com_y = (total != 0) ? (top - bottom) / total : 0;

  const int x = (int)((com_x + 1) * width() / 2);
  const int y = (int)((1 - com_y) * height() / 2);

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
  const double total = TotalWeight();
  if (event->button() == Qt::RightButton)
  {
    m_top_right = total / 4;
    m_bottom_right = total / 4;
    m_top_left = total / 4;
    m_bottom_left = total / 4;
  }
  else
  {
    // convert from widget space to value space
    const double com_x = std::clamp((event->pos().x() * 2.) / width() - 1, -1., 1.);
    const double com_y = std::clamp(1 - (event->pos().y() * 2.) / height(), -1., 1.);

    m_top_right = total * (1 + com_x + com_y) / 4;
    m_bottom_right = total * (1 + com_x - com_y) / 4;
    m_top_left = total * (1 - com_x + com_y) / 4;
    m_bottom_left = total * (1 - com_x - com_y) / 4;
  }

  if (m_reentrance_level < MAX_REENTRANCE)
  {
    const ReentranceBlocker blocker(this);
    emit ChangedTR(m_top_right);
    emit ChangedBR(m_bottom_right);
    emit ChangedTL(m_top_left);
    emit ChangedBL(m_bottom_left);
  }
  update();
}
