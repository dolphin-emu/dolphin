// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ToolTipControls/BalloonTip.h"

#include <memory>

#include <QBitmap>
#include <QBrush>
#include <QCursor>
#include <QFont>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QString>
#include <QVBoxLayout>

#if defined(__APPLE__)
#include <QToolTip>
#endif

#include "DolphinQt/Settings.h"

namespace
{
std::unique_ptr<BalloonTip> s_the_balloon_tip = nullptr;
}  // namespace

void BalloonTip::ShowBalloon(const QString& title, const QString& message,
                             const QPoint& target_arrow_tip_position, QWidget* const parent,
                             const ShowArrow show_arrow, const int border_width)
{
  HideBalloon();
  if (message.isEmpty() && title.isEmpty())
    return;

#if defined(__APPLE__)
  QString the_message = message;
  the_message.replace(QStringLiteral("<dolphin_emphasis>"), QStringLiteral("<b>"));
  the_message.replace(QStringLiteral("</dolphin_emphasis>"), QStringLiteral("</b>"));
  QToolTip::showText(target_arrow_tip_position, the_message, parent);
#else
  s_the_balloon_tip = std::make_unique<BalloonTip>(PrivateTag{}, title, message, parent);
  s_the_balloon_tip->UpdateBoundsAndRedraw(target_arrow_tip_position, show_arrow, border_width);
#endif
}

void BalloonTip::HideBalloon()
{
#if defined(__APPLE__)
  QToolTip::hideText();
#else
  if (s_the_balloon_tip == nullptr)
    return;
  s_the_balloon_tip->hide();
  s_the_balloon_tip.reset();
#endif
}

BalloonTip::BalloonTip(PrivateTag, const QString& title, QString message, QWidget* const parent)
    : QWidget(nullptr, Qt::ToolTip)
{
  QColor window_color;
  QColor text_color;
  QColor dolphin_emphasis;
  Settings::Instance().GetToolTipStyle(window_color, text_color, dolphin_emphasis, m_border_color,
                                       parent->palette(), palette());
  const auto style_sheet = QStringLiteral("background-color: #%1; color: #%2;")
                               .arg(window_color.rgba(), 0, 16)
                               .arg(text_color.rgba(), 0, 16);
  setStyleSheet(style_sheet);

  // Replace text in our our message if specific "tags" are used
  message.replace(QStringLiteral("<dolphin_emphasis>"),
                  QStringLiteral("<font color=\"#%1\"><b>").arg(dolphin_emphasis.rgba(), 0, 16));
  message.replace(QStringLiteral("</dolphin_emphasis>"), QStringLiteral("</b></font>"));

  auto* const balloontip_layout = new QVBoxLayout;
  balloontip_layout->setSizeConstraint(QLayout::SetFixedSize);
  setLayout(balloontip_layout);

  const auto create_label = [=](const QString& text) {
    QLabel* const label = new QLabel;
    balloontip_layout->addWidget(label);

    label->setText(text);
    label->setTextFormat(Qt::RichText);

    const int max_width = label->screen()->availableGeometry().width() / 3;
    label->setMaximumWidth(max_width);
    if (label->sizeHint().width() > max_width)
      label->setWordWrap(true);

    return label;
  };

  if (!title.isEmpty())
  {
    QLabel* const title_label = create_label(title);

    QFont title_font = title_label->font();
    title_font.setBold(true);
    title_label->setFont(title_font);
  }

  if (!message.isEmpty())
    create_label(message);
}

void BalloonTip::paintEvent(QPaintEvent*)
{
  QPainter painter(this);
  painter.drawPixmap(rect(), m_pixmap);
}

void BalloonTip::UpdateBoundsAndRedraw(const QPoint& target_arrow_tip_position,
                                       const ShowArrow show_arrow, const int border_full_width)
{
  const float border_half_width = border_full_width / 2.0;

  // This should be odd so that the arrow tip is a single pixel wide.
  constexpr int arrow_full_width = 35;
  constexpr float arrow_half_width = arrow_full_width / 2.0;
  // The y distance between the inner edge of the rectangle border and the inner tip of the arrow
  // border, and also the distance between the outer edge of the rectangle border and the outer tip
  // of the arrow border
  constexpr int arrow_height = (1 + arrow_full_width) / 2;

  // Distance between the label layout and the inner rectangle border edge
  constexpr int balloon_interior_padding = 12;
  // Prevent the corners of the label layout from portruding into the rounded rectangle corners at
  // larger border sizes.
  const int rounded_corner_margin = border_half_width / 4;
  const int horizontal_margin =
      border_full_width + rounded_corner_margin + balloon_interior_padding;
  const int vertical_margin = horizontal_margin + arrow_height;

  // Create enough space around the layout containing the title and message to draw the balloon and
  // both arrow tips (at most one of which will be visible)
  layout()->setContentsMargins(horizontal_margin, vertical_margin, horizontal_margin,
                               vertical_margin);

  QSize size_hint = sizeHint();

  // These positions represent the middle of each edge of the BalloonTip's rounded rectangle
  const float rect_width = size_hint.width() - border_full_width;
  const float rect_height = size_hint.height() - border_full_width - 2 * arrow_height;
  const float rect_top = border_half_width + arrow_height;
  const float rect_bottom = rect_top + rect_height;
  const float rect_left = border_half_width;
  // rect_right isn't used for anything

  // Qt defines the radius of a rounded rectangle as "the radius of the ellipses defining the
  // corner". Unlike the rectangle's edges this corresponds to the outside of the rounded curve
  // instead of its middle, so we add the full width to the inner radius instead of the half width
  constexpr float corner_base_inner_radius = 7.0;
  const float corner_outer_radius = corner_base_inner_radius + border_full_width;

  // This value is arbitrary but works well.
  constexpr int base_arrow_x_offset = 34;
  // Adjust the offset inward to compensate for the border and rounded corner widths. This ensures
  // the arrow is on the flat part of the top/bottom border.
  const int adjusted_arrow_x_offset =
      base_arrow_x_offset + border_full_width + static_cast<int>(border_half_width);
  // If the border is wide enough (or the BalloonTip small enough) the offset might end up past the
  // midpoint; if that happens just use the midpoint instead
  const int centered_arrow_x_offset = (size_hint.width() - arrow_full_width) / 2;
  // If the arrow is on the left this is the distance between the left edge of the BalloonTip and
  // the left edge of the arrow interior; otherwise the distance between the right edges.
  const int arrow_nearest_edge_x_offset =
      std::min(adjusted_arrow_x_offset, centered_arrow_x_offset);
  const int arrow_tip_x_offset = arrow_nearest_edge_x_offset + arrow_half_width;

  // The BalloonTip should be contained entirely within the screen that contains the target
  // position.
  QScreen* screen = QGuiApplication::screenAt(target_arrow_tip_position);
  if (screen == nullptr)
  {
    // If the target position isn't on any screen (which can happen if the window is partly off the
    // screen and the user hovers over the label) then use the screen containing the cursor instead.
    screen = QGuiApplication::screenAt(QCursor::pos());
  }
  const QRect screen_rect = screen->geometry();

  QPainterPath rect_path;
  rect_path.addRoundedRect(rect_left, rect_top, rect_width, rect_height, corner_outer_radius,
                           corner_outer_radius);

  // The default pen cap style Qt::SquareCap extends drawn lines one half width before the starting
  // point and one half width after the ending point such that the starting and ending points are
  // surrounded by drawn pixels in both dimensions instead of just for the width. This is a
  // reasonable default but we need to draw lines precisely, and this behavior causes problems when
  // drawing lines of length and width 1 (which we do for the arrow interior, and also the arrow
  // border when border_full_width is 1). For those lines to correctly end up as a pixel we would
  // need to offset the start and end points by 0.5 inward. However, doing that would lead them to
  // be at the same point, and if the endpoints of a line are the same Qt simply doesn't draw it
  // regardless of the cap style.
  //
  // Using Qt::FlatCap instead fixes the issue.

  m_pixmap = QPixmap(size_hint);

  QPen border_pen(m_border_color, border_full_width);
  border_pen.setCapStyle(Qt::FlatCap);

  QPainter balloon_painter(&m_pixmap);
  balloon_painter.setPen(border_pen);
  balloon_painter.setBrush(palette().color(QPalette::Window));
  balloon_painter.drawPath(rect_path);

  QBitmap mask_bitmap(size_hint);
  mask_bitmap.fill(Qt::color0);

  QPen mask_pen(Qt::color1, border_full_width);
  mask_pen.setCapStyle(Qt::FlatCap);

  QPainter mask_painter(&mask_bitmap);
  mask_painter.setPen(mask_pen);
  mask_painter.setBrush(QBrush(Qt::color1));
  mask_painter.drawPath(rect_path);

  const bool arrow_at_bottom =
      target_arrow_tip_position.y() - size_hint.height() + arrow_height >= 0;
  const bool arrow_at_left =
      target_arrow_tip_position.x() + size_hint.width() - arrow_tip_x_offset < screen_rect.width();

  const float arrow_base_y =
      arrow_at_bottom ? rect_bottom - border_half_width : rect_top + border_half_width;

  const float arrow_tip_vertical_offset = arrow_at_bottom ? arrow_height : -arrow_height;
  const float arrow_tip_interior_y = arrow_base_y + arrow_tip_vertical_offset;
  const float arrow_tip_exterior_y =
      arrow_tip_interior_y + (arrow_at_bottom ? border_full_width : -border_full_width);
  const float arrow_base_left_edge_x =
      arrow_at_left ? arrow_nearest_edge_x_offset :
                      size_hint.width() - arrow_nearest_edge_x_offset - arrow_full_width;
  const float arrow_base_right_edge_x = arrow_base_left_edge_x + arrow_full_width;
  const float arrow_tip_x = arrow_base_left_edge_x + arrow_half_width;

  if (show_arrow == ShowArrow::Yes)
  {
    // Drawing diagonal lines in Qt is filled with edge cases and inexplicable behavior. Getting it
    // to do what you want at one border size is simple enough, but doing so flexibly is an exercise
    // in futility. Some examples:
    // * For some values of x, diagonal lines of width x and x+1 are drawn exactly the same.
    // * When drawing a triangle where p1 and p3 have exactly the same y value, they can be drawn at
    // different heights.
    // * Lines of width 1 sometimes get drawn one pixel past where they should even with FlatCap,
    // but only on the left side (regardless of which direction the stroke was).
    //
    // Instead of dealing with all that, fake it with vertical lines which are much better behaved.
    // Draw a bunch of vertical lines with width 1 to form the arrow border and interior.

    QPainterPath arrow_border_path;
    QPainterPath arrow_interior_fill_path;
    const float y_end_offset = arrow_at_bottom ? border_full_width : -border_full_width;

    // Draw the arrow border and interior lines from the outside inward. Each loop iteration draws
    // one pair of border lines and one pair of interior lines.
    for (int i = 1; i <= arrow_half_width; i++)
    {
      const float x_offset_from_arrow_base_edge = i - 0.5;
      const float border_y_start = arrow_base_y + (arrow_at_bottom ? i : -i);
      const float border_y_end = border_y_start + y_end_offset;
      const float interior_y_start = arrow_base_y;
      const float interior_y_end = border_y_start;
      const float left_line_x = arrow_base_left_edge_x + x_offset_from_arrow_base_edge;
      const float right_line_x = arrow_base_right_edge_x - x_offset_from_arrow_base_edge;

      arrow_border_path.moveTo(left_line_x, border_y_start);
      arrow_border_path.lineTo(left_line_x, border_y_end);

      arrow_border_path.moveTo(right_line_x, border_y_start);
      arrow_border_path.lineTo(right_line_x, border_y_end);

      arrow_interior_fill_path.moveTo(left_line_x, interior_y_start);
      arrow_interior_fill_path.lineTo(left_line_x, interior_y_end);

      arrow_interior_fill_path.moveTo(right_line_x, interior_y_start);
      arrow_interior_fill_path.lineTo(right_line_x, interior_y_end);
    }
    // The middle border line
    arrow_border_path.moveTo(arrow_tip_x, arrow_tip_interior_y);
    arrow_border_path.lineTo(arrow_tip_x, arrow_tip_interior_y + y_end_offset);

    // The middle interior line
    arrow_interior_fill_path.moveTo(arrow_tip_x, arrow_base_y);
    arrow_interior_fill_path.lineTo(arrow_tip_x, arrow_tip_interior_y);

    border_pen.setWidth(1);

    balloon_painter.setPen(border_pen);
    balloon_painter.drawPath(arrow_border_path);

    QPen arrow_interior_fill_pen(palette().color(QPalette::Window), 1);
    arrow_interior_fill_pen.setCapStyle(Qt::FlatCap);
    balloon_painter.setPen(arrow_interior_fill_pen);
    balloon_painter.drawPath(arrow_interior_fill_path);

    mask_pen.setWidth(1);
    mask_painter.setPen(mask_pen);

    mask_painter.drawPath(arrow_border_path);
    mask_painter.drawPath(arrow_interior_fill_path);
  }

  setMask(mask_bitmap);

  // Place the arrow tip at the target position whether the arrow tip is drawn or not
  const int target_balloontip_global_x =
      target_arrow_tip_position.x() - static_cast<int>(arrow_tip_x);
  const int rightmost_valid_balloontip_global_x =
      screen_rect.left() + screen_rect.width() - size_hint.width();
  // If the balloon would extend off the screen, push it left or right until it's not
  const int actual_balloontip_global_x =
      std::max(screen_rect.left(),
               std::min(rightmost_valid_balloontip_global_x, target_balloontip_global_x));
  // The tip pixel should be in the middle of the control, and arrow_tip_exterior_y is at the bottom
  // of that pixel. When arrow_at_bottom is true the arrow is above arrow_tip_exterior_y and so the
  // tip pixel is in the right place, but when it's false the arrow is below arrow_tip_exterior_y
  // so the tip pixel would be the one below that. Make this adjustment to fix that.
  const int tip_pixel_adjustment = arrow_at_bottom ? 0 : 1;
  const int actual_balloontip_global_y =
      target_arrow_tip_position.y() - arrow_tip_exterior_y - tip_pixel_adjustment;

  move(actual_balloontip_global_x, actual_balloontip_global_y);

  show();
}
