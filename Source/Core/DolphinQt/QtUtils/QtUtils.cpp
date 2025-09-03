// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/QtUtils.h"

#include <QDateTimeEdit>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>

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

void AdjustSizeWithinScreen(QWidget* widget)
{
  const auto screen_size = widget->screen()->availableSize();

  const auto adj_screen_size = screen_size * 9 / 10;

  widget->resize(widget->sizeHint().boundedTo(adj_screen_size));

  CenterOnParentWindow(widget);
}

void CenterOnParentWindow(QWidget* const widget)
{
  // Find the top-level ancestor widget.
  const QWidget* ancestor{widget->parentWidget()};
  if (!ancestor)
    return;
  while (ancestor->parentWidget())
  {
    ancestor = ancestor->parentWidget();
  }

  // Calculate position based on the widgets' size and position.
  const QRect ancestor_geometry{ancestor->geometry()};
  const QSize ancestor_size{ancestor_geometry.size()};
  const QPoint ancestor_pos{ancestor_geometry.topLeft()};
  const QRect geometry{widget->geometry()};
  const QSize size{geometry.size()};
  const QPoint offset{(ancestor_size.width() - size.width()) / 2,
                      (ancestor_size.height() - size.height()) / 2};
  const QPoint pos{ancestor_pos + offset};

  widget->setGeometry(QRect(pos, size));
}

}  // namespace QtUtils
