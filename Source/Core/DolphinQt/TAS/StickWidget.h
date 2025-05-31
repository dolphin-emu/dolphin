// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

class StickWidget : public QWidget
{
  Q_OBJECT
public:
  explicit StickWidget(QWidget* parent, u16 max_x, u16 max_y);

signals:
  void ChangedX(u16 x);
  void ChangedY(u16 y);

public slots:
  void SetX(u16 x);
  void SetY(u16 y);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void handleMouseEvent(QMouseEvent* event);

private:
  u16 m_max_x;
  u16 m_max_y;
  u16 m_x = 0;
  u16 m_y = 0;
  bool m_ignore_movement = false;
};
