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

}  // namespace QtUtils
