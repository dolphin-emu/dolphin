// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/QtUtils/ElidedButton.h"

#include <QFontMetrics>
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

void ElidedButton::paintEvent(QPaintEvent* event)
{
  QStyleOptionButton option;
  initStyleOption(&option);

  option.text = fontMetrics().elidedText(
      text(), m_elide_mode,
      style()->subElementRect(QStyle::SE_PushButtonContents, &option, this).width());

  QStylePainter{this}.drawControl(QStyle::CE_PushButton, option);
}
