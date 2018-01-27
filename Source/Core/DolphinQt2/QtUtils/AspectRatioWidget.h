// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QBoxLayout;

class AspectRatioWidget : public QWidget
{
public:
  AspectRatioWidget(QWidget* widget, float width, float height, QWidget* parent = 0);
  void resizeEvent(QResizeEvent* event);

private:
  QBoxLayout* m_layout;
  float m_ar_width;
  float m_ar_height;
};
