// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QImage>
#include <QPainter>
#include <QWidget>

class CameraWidget : public QWidget
{
  Q_OBJECT
public:
  explicit CameraWidget(QWidget* parent = nullptr);
  void setFrame(const QImage& frame);
  void setBackgroundColor(const QColor& color);

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  QImage m_frame;
  QColor m_bg_color;
};
