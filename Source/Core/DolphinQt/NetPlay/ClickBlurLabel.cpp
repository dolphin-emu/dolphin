// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/ClickBlurLabel.h"

#include <QChar>
#include <QGraphicsBlurEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QString>
#include <QWidget>

#include "Common/CommonTypes.h"
#include "Common/Random.h"

ClickBlurLabel::ClickBlurLabel(QWidget* parent)
    : QStackedWidget(parent), m_normal_label(new QLabel(this)), m_blurred_label(new QLabel(this))
{
  setCursor(Qt::PointingHandCursor);

  // We use a QStackedWidget with a pre-blurred label instead of applying QGraphicsBlurEffect on
  // click, because creating the blur effect on demand can cause a visible delay on lower-end
  // hardware.
  auto* blur = new QGraphicsBlurEffect(m_blurred_label);
  blur->setBlurRadius(7);
  m_blurred_label->setGraphicsEffect(blur);

  // We don't want to take up more space than the labels take
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  addWidget(m_blurred_label);
  addWidget(m_normal_label);
}

void ClickBlurLabel::setText(const QString& text)
{
  if (this->text() == text)
    return;

  m_normal_label->setText(text);
  m_blurred_label->setText(GenerateBlurredText(text));
}

void ClickBlurLabel::mousePressEvent(QMouseEvent* event)
{
  int current = currentIndex();
  setCurrentIndex(current == 0 ? 1 : 0);
  QWidget::mousePressEvent(event);
}

QString ClickBlurLabel::GenerateBlurredText(const QString& text)
{
  QString blurred_text;
  blurred_text.reserve(text.size());
  for (const QChar& c : text)
  {
    if (c.isLetter())
      blurred_text += QChar((c.isUpper() ? 'A' : 'a') + (Common::Random::GenerateValue<u8>() % 26));
    else if (c.isDigit())
      blurred_text += QChar('0' + (Common::Random::GenerateValue<u8>() % 10));
    else
      blurred_text += c;
  }
  return blurred_text;
}
