// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QGraphicsBlurEffect>
#include <QLabel>
#include <qcontainerfwd.h>
#include <qpixmap.h>

class ClickBlurLabel final : public QLabel
{
  Q_OBJECT
public:
  explicit ClickBlurLabel(QWidget* parent = nullptr) : QLabel(parent)
  {
    setCursor(Qt::PointingHandCursor);
  }

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

private:
  QPixmap TextToPixmap(const QString& text);
  // Generates text that "looks correct" but is actually gibberish.
  QString GenerateBlurredText(const QString& text);
  QPixmap GenerateBlurredPixmap(const QString& text);
  // Calculates the position for the blurred pixmap
  // This is required for it to follow the label's alignment.
  QPoint CalculateAlignedQPoint();

  QPixmap blurred_pixmap;
  bool revealed = false;
  QString last_text;
};
