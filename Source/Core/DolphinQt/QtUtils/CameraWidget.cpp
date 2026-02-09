// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "CameraWidget.h"

CameraWidget::CameraWidget(QWidget* parent) : QWidget(parent), m_bg_color(Qt::darkGray)
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void CameraWidget::setFrame(const QImage& frame)
{
  m_frame = frame;
  update();
}

void CameraWidget::setBackgroundColor(const QColor& color)
{
  m_bg_color = color;
  update();
}

void CameraWidget::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);
  painter.fillRect(rect(), m_bg_color);

  if (!m_frame.isNull())
  {
    const QImage scaled = m_frame.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QPoint top_left((width() - scaled.width()) / 2, (height() - scaled.height()) / 2);
    painter.drawImage(top_left, scaled);
  }
}
