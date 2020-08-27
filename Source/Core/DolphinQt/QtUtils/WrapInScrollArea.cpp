// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include <QFrame>
#include <QLayout>
#include <QPalette>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

QWidget* GetWrappedWidget(QWidget* wrapped_widget, QWidget* to_resize, int margin_width,
                          int margin_height)
{
  auto* scroll = new QScrollArea;
  scroll->setWidget(wrapped_widget);
  scroll->setWidgetResizable(true);
  scroll->setFrameStyle(QFrame::NoFrame);

  if (to_resize != nullptr)
  {
    // For some reason width() is bigger than it needs to be.
    auto min_size = wrapped_widget->minimumSizeHint();
    int recommended_width = min_size.width() + margin_width;
    int recommended_height = min_size.height() + margin_height;

    to_resize->resize(std::max(recommended_width, to_resize->width()),
                      std::max(recommended_height, to_resize->height()));
  }

  scroll->viewport()->setAutoFillBackground(false);
  wrapped_widget->setAutoFillBackground(false);

  return scroll;
}

void WrapInScrollArea(QWidget* parent, QLayout* wrapped_layout, QWidget* to_resize)
{
  if (to_resize == nullptr)
    to_resize = parent;

  auto* widget = new QWidget;
  widget->setLayout(wrapped_layout);

  auto* scroll_area = GetWrappedWidget(widget, to_resize, 0, 0);

  auto* scroll_layout = new QVBoxLayout;
  scroll_layout->addWidget(scroll_area);
  scroll_layout->setMargin(0);

  parent->setLayout(scroll_layout);
}
