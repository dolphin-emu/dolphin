// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QStyle>

class QDateTimeEdit;
class QLabel;
class QWidget;

namespace QtUtils
{

void ShowFourDigitYear(QDateTimeEdit* widget);

QWidget* CreateIconWarning(QWidget* parent, QStyle::StandardPixmap standard_pixmap, QLabel* label);

// Similar to QWidget::adjustSize except maximum size is 9/10 of screen rather than 2/3.
void AdjustSizeWithinScreen(QWidget* widget);

// Centers the widget on its parent. It should be called after any adjustments to the widget's size
// has been applied.
void CenterOnParentWindow(QWidget* widget);

// A QWidget that returns the minimumSizeHint as the primary sizeHint.
// Useful for QListWidget which hints a fairly large height even when entirely empty.
// Usage: QtUtils::MinimumSizeHintWidget<QListWidget>
template <typename Widget>
class MinimumSizeHintWidget : public Widget
{
public:
  using Widget::Widget;

  // Note: Some widget (e.g. QPushButton) minimumSizeHint implementations themselves use sizeHint,
  //  which would cause this to stack overflow.
  QSize sizeHint() const override { return Widget::minimumSizeHint(); }
};

}  // namespace QtUtils
