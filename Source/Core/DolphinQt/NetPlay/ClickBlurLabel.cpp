// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/NetPlay/ClickBlurLabel.h"

#include <QGraphicsEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QLabel>
#include <QPainter>
#include <QPixmap>

#include "Common/Random.h"

void ClickBlurLabel::mousePressEvent(QMouseEvent* event)
{
  revealed = !revealed;
  update();
  QLabel::mousePressEvent(event);
}

void ClickBlurLabel::paintEvent(QPaintEvent* event)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  if (revealed)
  {
    QLabel::paintEvent(event);
    return;
  }

  const QString current_text = text();
  if (last_text != current_text)
  {
    blurred_pixmap = GenerateBlurredPixmap(current_text);
    last_text = current_text;
  }

  painter.drawPixmap(CalculateAlignedQPoint(), blurred_pixmap);
}

QPixmap ClickBlurLabel::TextToPixmap(const QString& text)
{
  const QFont font = this->font();
  QFontMetrics fm(font);
  const QSize size = fm.size(Qt::TextSingleLine, text);
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setFont(font);
  painter.setPen(palette().color(QPalette::WindowText));
  painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
  return pixmap;
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

QPixmap ClickBlurLabel::GenerateBlurredPixmap(const QString& text)
{
  QPixmap pixmap = TextToPixmap(GenerateBlurredText(text));

  QGraphicsScene scene;
  QGraphicsPixmapItem item(pixmap);

  QGraphicsBlurEffect blur;
  blur.setBlurRadius(7);
  item.setGraphicsEffect(&blur);

  scene.addItem(&item);

  QImage blurred_image(pixmap.size(), QImage::Format_ARGB32_Premultiplied);
  blurred_image.fill(Qt::transparent);

  QPainter painter(&blurred_image);
  scene.render(&painter);

  return QPixmap::fromImage(blurred_image);
}

QPoint ClickBlurLabel::CalculateAlignedQPoint()
{
  Qt::Alignment alignment = this->alignment();
  int x;
  int y;

  if (alignment & Qt::AlignRight)
    x = width() - blurred_pixmap.width();
  else if (alignment & Qt::AlignHCenter)
    x = (width() - blurred_pixmap.width()) / 2;
  else
    x = 0;

  if (alignment & Qt::AlignBottom)
    y = height() - blurred_pixmap.height();
  else if (alignment & Qt::AlignVCenter)
    y = (height() - blurred_pixmap.height()) / 2;
  else
    y = 0;

  return QPoint(x, y);
}
