// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

class IRWidget : public QWidget
{
  Q_OBJECT
public:
  explicit IRWidget(QWidget* parent, s16 min_x, s16 max_x, s16 min_y, s16 max_y);

signals:
  void ChangedX(s16 x);
  void ChangedY(s16 y);

public slots:
  void SetX(s16 x);
  void SetY(s16 y);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void handleMouseEvent(QMouseEvent* event);

private:
  s16 m_x = 0;
  s16 m_y = 0;
  s16 m_min_x;
  s16 m_max_x;
  s16 m_min_y;
  s16 m_max_y;
  bool m_ignore_movement = false;
};
