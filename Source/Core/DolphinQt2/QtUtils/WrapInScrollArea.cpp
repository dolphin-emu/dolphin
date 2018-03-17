// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/QtUtils/WrapInScrollArea.h"

#include <QFrame>
#include <QLayout>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

QWidget* GetWrappedWidget(QWidget* wrapped_widget, QWidget* to_resize, int margin)
{
  auto* scroll = new QScrollArea;
  scroll->setWidget(wrapped_widget);
  scroll->setWidgetResizable(true);
  scroll->setFrameStyle(QFrame::NoFrame);

  if (to_resize != nullptr)
  {
    // For some reason width() is bigger than it needs to be.
    int recommended_width = wrapped_widget->width() * 0.9;
    int recommended_height = wrapped_widget->height() + margin;

    to_resize->resize(std::max(recommended_width, to_resize->width()),
                      std::max(recommended_height, to_resize->height()));
  }

  return scroll;
}

void WrapInScrollArea(QWidget* parent, QLayout* wrapped_layout, QWidget* to_resize)
{
  if (to_resize == nullptr)
    to_resize = parent;

  auto* widget = new QWidget;
  widget->setLayout(wrapped_layout);

  auto* scroll_area = GetWrappedWidget(widget, to_resize);

  auto* scroll_layout = new QVBoxLayout;
  scroll_layout->addWidget(scroll_area);
  scroll_layout->setMargin(0);

  parent->setLayout(scroll_layout);
}
