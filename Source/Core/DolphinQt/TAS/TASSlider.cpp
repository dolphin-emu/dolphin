// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/TASSlider.h"

#include <QMouseEvent>

TASSlider::TASSlider(int default_, QWidget* parent) : QSlider(parent), m_default(default_)
{
}

TASSlider::TASSlider(int default_, Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent), m_default(default_)
{
}

void TASSlider::mouseReleaseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::RightButton)
    setValue(m_default);
  else
    QSlider::mouseReleaseEvent(event);
}
