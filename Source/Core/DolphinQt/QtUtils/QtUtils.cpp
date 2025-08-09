// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/QtUtils.h"

#include <QDateTimeEdit>

namespace QtUtils
{

void ShowFourDigitYear(QDateTimeEdit* widget)
{
  if (!widget->displayFormat().contains(QStringLiteral("yyyy")))
  {
    // Always show the full year, no matter what the locale specifies. Otherwise, two-digit years
    // will always be interpreted as in the 21st century.
    widget->setDisplayFormat(
        widget->displayFormat().replace(QStringLiteral("yy"), QStringLiteral("yyyy")));
  }
}

}  // namespace QtUtils
