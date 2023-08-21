// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QBoxLayout;

class AspectRatioWidget : public QWidget
{
  Q_OBJECT
public:
  AspectRatioWidget(QWidget* widget, float width, float height, QWidget* parent = nullptr);
  void resizeEvent(QResizeEvent* event) override;

private:
  QBoxLayout* m_layout;
  float m_ar_width;
  float m_ar_height;
};
