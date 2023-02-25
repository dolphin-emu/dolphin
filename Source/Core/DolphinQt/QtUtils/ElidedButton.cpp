// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/ElidedButton.h"

#include <QStyleOptionButton>
#include <QStylePainter>

ElidedButton::ElidedButton(const QString& text, Qt::TextElideMode elide_mode)
    : QPushButton(text, nullptr), m_elide_mode{elide_mode}
{
}

Qt::TextElideMode ElidedButton::elideMode() const
{
  return m_elide_mode;
}

void ElidedButton::setElideMode(Qt::TextElideMode elide_mode)
{
  if (elide_mode == m_elide_mode)
    return;

  m_elide_mode = elide_mode;
  repaint();
}

QSize ElidedButton::sizeHint() const
{
  // Long text produces big sizeHints which is throwing layouts off
  // even when setting fixed sizes. This seems like a Qt layout bug.
  // Let's always return the sizeHint of an empty button to work around this.
  return QPushButton(parentWidget()).sizeHint();
}

void ElidedButton::paintEvent(QPaintEvent* event)
{
  QStyleOptionButton option;
  initStyleOption(&option);

  option.text = fontMetrics().elidedText(
      text(), m_elide_mode,
      style()->subElementRect(QStyle::SE_PushButtonContents, &option, this).width());

  QStylePainter{this}.drawControl(QStyle::CE_PushButton, option);
}
