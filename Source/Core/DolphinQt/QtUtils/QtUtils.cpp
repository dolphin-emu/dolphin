// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/QtUtils.h"

#include <QDateTimeEdit>
#include <QHBoxLayout>
#include <QLabel>

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

QWidget* CreateIconWarning(QWidget* parent, QStyle::StandardPixmap standard_pixmap, QLabel* label)
{
  const auto size = QFontMetrics(parent->font()).height() * 5 / 4;

  auto* const icon = new QLabel{};
  icon->setPixmap(parent->style()->standardIcon(standard_pixmap).pixmap(size, size));

  auto* const widget = new QWidget;
  auto* const layout = new QHBoxLayout{widget};
  layout->addWidget(icon);
  layout->addWidget(label, 1);
  return widget;
}

}  // namespace QtUtils
