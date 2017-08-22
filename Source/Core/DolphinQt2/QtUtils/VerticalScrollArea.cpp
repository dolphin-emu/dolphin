// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/QtUtils/VerticalScrollArea.h"

#include <QEvent>
#include <QScrollBar>
#include <QVBoxLayout>

VerticalScrollArea::VerticalScrollArea(QWidget* parent) : QScrollArea(parent)
{
  setWidgetResizable(true);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

QSize VerticalScrollArea::sizeHint() const
{
  return {QScrollArea::sizeHint().width(), widget()->minimumSizeHint().height()};
}

QLayout* WrapInVerticalScrollArea(QLayout* layout)
{
  QWidget* scroll_widget = new QWidget();
  scroll_widget->setLayout(layout);

  auto* scroll_area = new VerticalScrollArea();
  scroll_area->setMinimumWidth(scroll_widget->minimumSizeHint().width() +
                               scroll_area->verticalScrollBar()->width());
  scroll_area->setFrameStyle(QFrame::NoFrame);

  auto original_margins = layout->contentsMargins();
  scroll_area->setWidget(scroll_widget);
  layout->setContentsMargins(original_margins);

  auto* scroll_layout = new QVBoxLayout();
  scroll_layout->addWidget(scroll_area);
  scroll_layout->setContentsMargins(0, 0, 0, 0);

  return scroll_layout;
}
