// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ToolTipControls/BalloonTip.h"

#include <memory>

#include <QBitmap>
#include <QGraphicsEffect>
#include <QGraphicsView>
#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyle>

#include <QScreen>

#if defined(__APPLE__)
#include <QToolTip>
#endif

#include "Core/Config/MainSettings.h"

#include "DolphinQt/Settings.h"

namespace
{
std::unique_ptr<BalloonTip> s_the_balloon_tip = nullptr;
}  // namespace

void BalloonTip::ShowBalloon(const QIcon& icon, const QString& title, const QString& message,
                             const QPoint& pos, QWidget* parent, ShowArrow show_arrow)
{
  HideBalloon();
  if (message.isEmpty() && title.isEmpty())
    return;

#if defined(__APPLE__)
  QString the_message = message;
  the_message.replace(QStringLiteral("<dolphin_emphasis>"), QStringLiteral("<b>"));
  the_message.replace(QStringLiteral("</dolphin_emphasis>"), QStringLiteral("</b>"));
  QToolTip::showText(pos, the_message, parent);
#else
  s_the_balloon_tip = std::make_unique<BalloonTip>(PrivateTag{}, icon, title, message, parent);
  s_the_balloon_tip->UpdateBoundsAndRedraw(pos, show_arrow);
#endif
}

void BalloonTip::HideBalloon()
{
#if defined(__APPLE__)
  QToolTip::hideText();
#else
  if (!s_the_balloon_tip)
    return;
  s_the_balloon_tip->hide();
  s_the_balloon_tip.reset();
#endif
}

BalloonTip::BalloonTip(PrivateTag, const QIcon& icon, QString title, QString message,
                       QWidget* parent)
    : QWidget(nullptr, Qt::ToolTip)
{
  setAttribute(Qt::WA_DeleteOnClose);
  setAutoFillBackground(true);

  QColor window_color;
  QColor text_color;
  QColor dolphin_emphasis;
  Settings::Instance().GetToolTipStyle(window_color, text_color, dolphin_emphasis, m_border_color,
                                       parent->palette(), palette());
  const auto style_sheet = QStringLiteral("background-color: #%1; color: #%2;")
                               .arg(window_color.rgba(), 0, 16)
                               .arg(text_color.rgba(), 0, 16);
  setStyleSheet(style_sheet);

  // Replace text in our our message
  // if specific "tags" are used
  message.replace(QStringLiteral("<dolphin_emphasis>"),
                  QStringLiteral("<font color=\"#%1\"><b>").arg(dolphin_emphasis.rgba(), 0, 16));
  message.replace(QStringLiteral("</dolphin_emphasis>"), QStringLiteral("</b></font>"));

  auto* title_label = new QLabel;
  title_label->installEventFilter(this);
  title_label->setText(title);
  QFont f = title_label->font();
  f.setBold(true);
  title_label->setFont(f);
  title_label->setTextFormat(Qt::RichText);
  title_label->setSizePolicy(QSizePolicy::Policy::MinimumExpanding,
                             QSizePolicy::Policy::MinimumExpanding);

  auto* message_label = new QLabel;
  message_label->installEventFilter(this);
  message_label->setText(message);
  message_label->setTextFormat(Qt::RichText);
  message_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);

  const int limit = message_label->screen()->availableGeometry().width() / 3;
  message_label->setMaximumWidth(limit);
  message_label->setSizePolicy(QSizePolicy::Policy::MinimumExpanding,
                               QSizePolicy::Policy::MinimumExpanding);
  if (message_label->sizeHint().width() > limit)
  {
    message_label->setWordWrap(true);
  }

  auto* layout = new QGridLayout;
  layout->addWidget(title_label, 0, 0, 1, 2);

  layout->addWidget(message_label, 1, 0, 1, 3);
  layout->setSizeConstraint(QLayout::SetMinimumSize);
  setLayout(layout);
}

void BalloonTip::paintEvent(QPaintEvent*)
{
  QPainter painter(this);
  painter.drawPixmap(rect(), m_pixmap);
}

void BalloonTip::UpdateBoundsAndRedraw(const QPoint& pos, ShowArrow show_arrow)
{
  m_show_arrow = show_arrow == ShowArrow::Yes;

  QScreen* screen = QGuiApplication::screenAt(pos);
  if (!screen)
    screen = QGuiApplication::primaryScreen();
  const QRect screen_rect = screen->geometry();

  QSize sh = sizeHint();
  // The look should resemble the default tooltip style set in Settings::SetCurrentUserStyle()
  const int border = 1;
  const int arrow_height = 18;
  const int arrow_width = 18;
  const int arrow_offset = 52;
  const int rect_center = 7;
  const bool arrow_at_bottom = (pos.y() - sh.height() - arrow_height > 0);
  const bool arrow_at_left = (pos.x() + sh.width() - arrow_width < screen_rect.width());
  const int default_padding = 10;
  layout()->setContentsMargins(border + 3 + default_padding,
                               border + (arrow_at_bottom ? 0 : arrow_height) + 2 + default_padding,
                               border + 3 + default_padding,
                               border + (arrow_at_bottom ? arrow_height : 0) + 2 + default_padding);
  updateGeometry();
  sh = sizeHint();

  int ml, mr, mt, mb;
  QSize sz = sizeHint();
  if (arrow_at_bottom)
  {
    ml = mt = 0;
    mr = sz.width() - 1;
    mb = sz.height() - arrow_height - 1;
  }
  else
  {
    ml = 0;
    mt = arrow_height;
    mr = sz.width() - 1;
    mb = sz.height() - 1;
  }

  QPainterPath path;
  path.moveTo(ml + rect_center, mt);
  if (!arrow_at_bottom && arrow_at_left)
  {
    if (m_show_arrow)
    {
      path.lineTo(ml + arrow_offset - arrow_width, mt);
      path.lineTo(ml + arrow_offset, mt - arrow_height);
      path.lineTo(ml + arrow_offset + arrow_width, mt);
    }
    move(qMax(pos.x() - arrow_offset, screen_rect.left() + 2), pos.y());
  }
  else if (!arrow_at_bottom && !arrow_at_left)
  {
    if (m_show_arrow)
    {
      path.lineTo(mr - arrow_offset - arrow_width, mt);
      path.lineTo(mr - arrow_offset, mt - arrow_height);
      path.lineTo(mr - arrow_offset + arrow_width, mt);
    }
    move(qMin(pos.x() - sh.width() + arrow_offset, screen_rect.right() - sh.width() - 2), pos.y());
  }
  path.lineTo(mr - rect_center, mt);
  path.arcTo(QRect(mr - rect_center * 2, mt, rect_center * 2, rect_center * 2), 90, -90);
  path.lineTo(mr, mb - rect_center);
  path.arcTo(QRect(mr - rect_center * 2, mb - rect_center * 2, rect_center * 2, rect_center * 2), 0,
             -90);
  if (arrow_at_bottom && !arrow_at_left)
  {
    if (m_show_arrow)
    {
      path.lineTo(mr - arrow_offset + arrow_width, mb);
      path.lineTo(mr - arrow_offset, mb + arrow_height);
      path.lineTo(mr - arrow_offset - arrow_width, mb);
    }
    move(qMin(pos.x() - sh.width() + arrow_offset, screen_rect.right() - sh.width() - 2),
         pos.y() - sh.height());
  }
  else if (arrow_at_bottom && arrow_at_left)
  {
    if (m_show_arrow)
    {
      path.lineTo(arrow_offset + arrow_width, mb);
      path.lineTo(arrow_offset, mb + arrow_height);
      path.lineTo(arrow_offset - arrow_width, mb);
    }
    move(qMax(pos.x() - arrow_offset, screen_rect.x() + 2), pos.y() - sh.height());
  }
  path.lineTo(ml + rect_center, mb);
  path.arcTo(QRect(ml, mb - rect_center * 2, rect_center * 2, rect_center * 2), -90, -90);
  path.lineTo(ml, mt + rect_center);
  path.arcTo(QRect(ml, mt, rect_center * 2, rect_center * 2), 180, -90);

  // Set the mask
  QBitmap bitmap(sizeHint());
  bitmap.fill(Qt::color0);
  QPainter painter1(&bitmap);
  painter1.setPen(QPen(Qt::color1, border));
  painter1.setBrush(QBrush(Qt::color1));
  painter1.drawPath(path);
  setMask(bitmap);

  // Draw the border
  m_pixmap = QPixmap(sz);
  QPainter painter2(&m_pixmap);
  painter2.setPen(QPen(m_border_color));
  painter2.setBrush(palette().color(QPalette::Window));
  painter2.drawPath(path);

  show();
}
