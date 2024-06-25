// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QColor>
#include <QPixmap>
#include <QWidget>

class QPaintEvent;
class QPoint;
class QString;

class BalloonTip : public QWidget
{
  Q_OBJECT

  struct PrivateTag
  {
  };

public:
  enum class ShowArrow
  {
    Yes,
    No
  };
  static void ShowBalloon(const QString& title, const QString& message,
                          const QPoint& target_arrow_tip_position, QWidget* parent,
                          ShowArrow show_arrow = ShowArrow::Yes, int border_width = 1);
  static void HideBalloon();
  static bool IsCursorInsideWidgetBoundingBox(const QWidget& widget);
  static bool IsCursorOnBalloonTip();
  static bool IsWidgetBalloonTipActive(const QWidget& widget);

  BalloonTip(PrivateTag, const QString& title, QString message, QWidget* parent);

protected:
  void enterEvent(QEnterEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
  void paintEvent(QPaintEvent*) override;

private:
  void UpdateBoundsAndRedraw(const QPoint& target_arrow_tip_position, ShowArrow show_arrow,
                             int border_width);

  QColor m_border_color;
  QPixmap m_pixmap;
};
