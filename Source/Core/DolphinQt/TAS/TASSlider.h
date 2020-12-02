// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QSlider>

class QMouseEvent;

class TASSlider : public QSlider
{
public:
  explicit TASSlider(int default_, QWidget* parent = nullptr);
  explicit TASSlider(int default_, Qt::Orientation orientation, QWidget* parent = nullptr);

protected:
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  int m_default;
};
