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

}  // namespace QtUtils
