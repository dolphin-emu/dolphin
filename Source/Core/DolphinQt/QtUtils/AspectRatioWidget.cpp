// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on:
// https://stackoverflow.com/questions/30005540/keeping-the-aspect-ratio-of-a-sub-classed-qwidget-during-resize

#include "DolphinQt/QtUtils/AspectRatioWidget.h"

#include <QBoxLayout>
#include <QResizeEvent>

AspectRatioWidget::AspectRatioWidget(QWidget* widget, float width, float height, QWidget* parent)
    : QWidget(parent), m_ar_width(width), m_ar_height(height)
{
  m_layout = new QBoxLayout(QBoxLayout::LeftToRight, this);

  // add spacer, then your widget, then spacer
  m_layout->addItem(new QSpacerItem(0, 0));
  m_layout->addWidget(widget);
  m_layout->addItem(new QSpacerItem(0, 0));
}

void AspectRatioWidget::resizeEvent(QResizeEvent* event)
{
  float aspect_ratio = static_cast<float>(event->size().width()) / event->size().height();
  int widget_stretch, outer_stretch;

  if (aspect_ratio > (m_ar_width / m_ar_height))  // too wide
  {
    m_layout->setDirection(QBoxLayout::LeftToRight);
    widget_stretch = height() * (m_ar_width / m_ar_height);  // i.e., my width
    outer_stretch = (width() - widget_stretch) / 2 + 0.5;
  }
  else  // too tall
  {
    m_layout->setDirection(QBoxLayout::TopToBottom);
    widget_stretch = width() * (m_ar_height / m_ar_width);  // i.e., my height
    outer_stretch = (height() - widget_stretch) / 2 + 0.5;
  }

  m_layout->setStretch(0, outer_stretch);
  m_layout->setStretch(1, widget_stretch);
  m_layout->setStretch(2, outer_stretch);
}
