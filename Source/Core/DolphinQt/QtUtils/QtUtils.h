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

// Similar to `QWidget::adjustSize()` except maximum size is either 8/10 of the screen (for screen
// resolutions greater than FullHD) or 9/10 of the screen (for smaller resolutions), as opposed to
// the 2/3 factor that is used in `QWidget::adjustSize()`.
void AdjustSizeWithinScreen(QWidget* widget);

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
