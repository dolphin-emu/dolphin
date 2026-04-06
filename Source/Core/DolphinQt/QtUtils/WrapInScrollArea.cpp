// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include <QFrame>
#include <QLayout>
#include <QPalette>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWidget>

namespace
{

// This scroll area prefers the size of its underlying widget.
class HintingScrollArea final : public QScrollArea
{
public:
  HintingScrollArea()
  {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setWidgetResizable(true);

    // Make things not look horrendous on Windows.
    setFrameStyle(QFrame::NoFrame);
  }

  QSize sizeHint() const override
  {
    // Including the scrollbars in our hint can prevent
    //  a window undersized in one direction gaining unnecessary scrolling in both directions.
    const auto scrollbar_padding =
        QSize{verticalScrollBar()->sizeHint().width(), horizontalScrollBar()->sizeHint().height()};

    const auto size_hint = widget()->sizeHint();

    return size_hint + scrollbar_padding;
  }
};

}  // namespace

QWidget* GetWrappedWidget(QWidget* wrapped_widget)
{
  auto* const scroll = new HintingScrollArea;
  scroll->setWidget(wrapped_widget);

  // Workaround for transparency issues on macOS. Not sure if this is still needed.
  scroll->viewport()->setAutoFillBackground(false);
  wrapped_widget->setAutoFillBackground(false);

  return scroll;
}

void WrapInScrollArea(QWidget* parent, QLayout* wrapped_layout)
{
  auto* widget = new QWidget;
  widget->setLayout(wrapped_layout);

  auto* scroll_area = GetWrappedWidget(widget);

  auto* scroll_layout = new QVBoxLayout;
  scroll_layout->addWidget(scroll_area);
  scroll_layout->setContentsMargins(0, 0, 0, 0);

  parent->setLayout(scroll_layout);
}
