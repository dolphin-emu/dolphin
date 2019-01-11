// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingIndicator.h"

#include <array>
#include <cmath>

#include <QPainter>
#include <QTimer>

#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/Device.h"

#include "DolphinQt/Settings.h"

// Color constants to keep things looking consistent:
// TODO: could we maybe query theme colors from Qt for the bounding box?
const QColor BBOX_PEN_COLOR = Qt::darkGray;
const QColor BBOX_BRUSH_COLOR = Qt::white;

const QColor RAW_INPUT_COLOR = Qt::darkGray;
const QColor ADJ_INPUT_COLOR = Qt::red;
const QPen INPUT_SHAPE_PEN(RAW_INPUT_COLOR, 1.0, Qt::DashLine);

const QColor DEADZONE_COLOR = Qt::darkGray;
const QBrush DEADZONE_BRUSH(DEADZONE_COLOR, Qt::BDiagPattern);

const QColor TEXT_COLOR = Qt::darkGray;
// Text color that is visible atop ADJ_INPUT_COLOR:
const QColor TEXT_ALT_COLOR = Qt::white;

const QColor STICK_GATE_COLOR = Qt::lightGray;
const QColor C_STICK_GATE_COLOR = Qt::yellow;
const QColor CURSOR_TV_COLOR = 0xaed6f1;
const QColor TILT_GATE_COLOR = 0xa2d9ce;

constexpr int INPUT_DOT_RADIUS = 2;

MappingIndicator::MappingIndicator(ControllerEmu::ControlGroup* group) : m_group(group)
{
  setMinimumHeight(128);

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, [this] { repaint(); });
  m_timer->start(1000 / 30);
}

namespace
{
// Constructs a polygon by querying a radius at varying angles:
template <typename F>
QPolygonF GetPolygonFromRadiusGetter(F&& radius_getter, double scale)
{
  // A multiple of 8 (octagon) and enough points to be visibly pleasing:
  constexpr int shape_point_count = 32;
  QPolygonF shape{shape_point_count};

  int p = 0;
  for (auto& point : shape)
  {
    const double angle = MathUtil::TAU * p / shape.size();
    const double radius = radius_getter(angle) * scale;

    point = {std::cos(angle) * radius, std::sin(angle) * radius};
    ++p;
  }

  return shape;
}
}  // namespace

void MappingIndicator::DrawCursor(ControllerEmu::Cursor& cursor)
{
  const QColor tv_brush_color = CURSOR_TV_COLOR;
  const QColor tv_pen_color = tv_brush_color.darker(125);

  // TODO: This SetControllerStateNeeded interface leaks input into the game
  // We should probably hold the mutex for UI updates.
  Settings::Instance().SetControllerStateNeeded(true);
  const auto raw_coord = cursor.GetState(false);
  const auto adj_coord = cursor.GetState(true);
  Settings::Instance().SetControllerStateNeeded(false);

  // Bounding box size:
  const double scale = height() / 2.5;

  QPainter p(this);
  p.translate(width() / 2, height() / 2);

  // Bounding box.
  p.setBrush(BBOX_BRUSH_COLOR);
  p.setPen(BBOX_PEN_COLOR);
  p.drawRect(-scale - 1, -scale - 1, scale * 2 + 1, scale * 2 + 1);

  // UI y-axis is opposite that of stick.
  p.scale(1.0, -1.0);

  // Enable AA after drawing bounding box.
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  // Deadzone for Z (forward/backward):
  const double deadzone = cursor.numeric_settings[cursor.SETTING_DEADZONE]->GetValue();
  if (deadzone > 0.0)
  {
    p.setPen(DEADZONE_COLOR);
    p.setBrush(DEADZONE_BRUSH);
    p.drawRect(QRectF(-scale, -deadzone * scale, scale * 2, deadzone * scale * 2));
  }

  // Raw Z:
  p.setPen(Qt::NoPen);
  p.setBrush(RAW_INPUT_COLOR);
  p.drawRect(
      QRectF(-scale, raw_coord.z * scale - INPUT_DOT_RADIUS / 2, scale * 2, INPUT_DOT_RADIUS));

  // Adjusted Z (if not hidden):
  if (adj_coord.z && adj_coord.x < 10000)
  {
    p.setBrush(ADJ_INPUT_COLOR);
    p.drawRect(
        QRectF(-scale, adj_coord.z * scale - INPUT_DOT_RADIUS / 2, scale * 2, INPUT_DOT_RADIUS));
  }

  // TV screen or whatever you want to call this:
  constexpr double tv_scale = 0.75;
  constexpr double center_scale = 2.0 / 3.0;

  const double tv_center = (cursor.numeric_settings[cursor.SETTING_CENTER]->GetValue() - 0.5);
  const double tv_width = cursor.numeric_settings[cursor.SETTING_WIDTH]->GetValue();
  const double tv_height = cursor.numeric_settings[cursor.SETTING_HEIGHT]->GetValue();

  p.setPen(tv_pen_color);
  p.setBrush(tv_brush_color);
  auto gate_polygon = GetPolygonFromRadiusGetter(
      [&cursor](double ang) { return cursor.GetGateRadiusAtAngle(ang); }, scale);
  for (auto& pt : gate_polygon)
  {
    pt = {pt.x() * tv_width, pt.y() * tv_height + tv_center * center_scale * scale};
    pt *= tv_scale;
  }
  p.drawPolygon(gate_polygon);

  // Deadzone.
  p.setPen(DEADZONE_COLOR);
  p.setBrush(DEADZONE_BRUSH);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&cursor](double ang) { return cursor.GetDeadzoneRadiusAtAngle(ang); }, scale));

  // Input shape.
  p.setPen(INPUT_SHAPE_PEN);
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&cursor](double ang) { return cursor.GetInputRadiusAtAngle(ang); }, scale));

  // Raw stick position.
  p.setPen(Qt::NoPen);
  p.setBrush(RAW_INPUT_COLOR);
  p.drawEllipse(QPointF{raw_coord.x, raw_coord.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);

  // Adjusted cursor position (if not hidden):
  if (adj_coord.x < 10000)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(ADJ_INPUT_COLOR);
    const QPointF pt(adj_coord.x / 2.0, (adj_coord.y - tv_center) / 2.0 + tv_center * center_scale);
    p.drawEllipse(pt * scale * tv_scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }
}

void MappingIndicator::DrawReshapableInput(ControllerEmu::ReshapableInput& stick)
{
  // Some hacks for pretty colors:
  const bool is_c_stick = m_group->name == "C-Stick";
  const bool is_tilt = m_group->name == "Tilt";

  QColor gate_brush_color = STICK_GATE_COLOR;

  if (is_c_stick)
    gate_brush_color = C_STICK_GATE_COLOR;
  else if (is_tilt)
    gate_brush_color = TILT_GATE_COLOR;

  const QColor gate_pen_color = gate_brush_color.darker(125);

  // TODO: This SetControllerStateNeeded interface leaks input into the game
  // We should probably hold the mutex for UI updates.
  Settings::Instance().SetControllerStateNeeded(true);
  const auto raw_coord = stick.GetReshapableState(false);
  const auto adj_coord = stick.GetReshapableState(true);
  Settings::Instance().SetControllerStateNeeded(false);

  // Bounding box size:
  const double scale = height() / 2.5;

  QPainter p(this);
  p.translate(width() / 2, height() / 2);

  // Bounding box.
  p.setBrush(BBOX_BRUSH_COLOR);
  p.setPen(BBOX_PEN_COLOR);
  p.drawRect(-scale - 1, -scale - 1, scale * 2 + 1, scale * 2 + 1);

  // UI y-axis is opposite that of stick.
  p.scale(1.0, -1.0);

  // Enable AA after drawing bounding box.
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  // Input gate. (i.e. the octagon shape)
  p.setPen(gate_pen_color);
  p.setBrush(gate_brush_color);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&stick](double ang) { return stick.GetGateRadiusAtAngle(ang); }, scale));

  // Deadzone.
  p.setPen(DEADZONE_COLOR);
  p.setBrush(DEADZONE_BRUSH);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&stick](double ang) { return stick.GetDeadzoneRadiusAtAngle(ang); }, scale));

  // Input shape.
  p.setPen(INPUT_SHAPE_PEN);
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&stick](double ang) { return stick.GetInputRadiusAtAngle(ang); }, scale));

  // Raw stick position.
  p.setPen(Qt::NoPen);
  p.setBrush(RAW_INPUT_COLOR);
  p.drawEllipse(QPointF{raw_coord.x, raw_coord.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);

  // Adjusted stick position.
  if (adj_coord.x || adj_coord.y)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(ADJ_INPUT_COLOR);
    p.drawEllipse(QPointF{adj_coord.x, adj_coord.y} * scale, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
  }
}

void MappingIndicator::DrawMixedTriggers()
{
  QPainter p(this);
  p.setRenderHint(QPainter::TextAntialiasing, true);

  const auto& triggers = *static_cast<ControllerEmu::MixedTriggers*>(m_group);
  const ControlState threshold = triggers.GetThreshold();
  const ControlState deadzone = triggers.GetDeadzone();

  // MixedTriggers interface is a bit ugly:
  constexpr int TRIGGER_COUNT = 2;
  std::array<ControlState, TRIGGER_COUNT> raw_analog_state;
  std::array<ControlState, TRIGGER_COUNT> adj_analog_state;
  const std::array<u16, TRIGGER_COUNT> button_masks = {0x1, 0x2};
  u16 button_state = 0;

  Settings::Instance().SetControllerStateNeeded(true);
  triggers.GetState(&button_state, button_masks.data(), raw_analog_state.data(), false);
  triggers.GetState(&button_state, button_masks.data(), adj_analog_state.data(), true);
  Settings::Instance().SetControllerStateNeeded(false);

  // Rectangle sizes:
  const int trigger_height = 32;
  const int trigger_width = width() - 1;
  const int trigger_button_width = 32;
  const int trigger_analog_width = trigger_width - trigger_button_width;

  // Bounding box background:
  p.setPen(Qt::NoPen);
  p.setBrush(BBOX_BRUSH_COLOR);
  p.drawRect(0, 0, trigger_width, trigger_height * TRIGGER_COUNT);

  for (int t = 0; t != TRIGGER_COUNT; ++t)
  {
    const double raw_analog = raw_analog_state[t];
    const double adj_analog = adj_analog_state[t];
    const bool trigger_button = button_state & button_masks[t];
    auto const analog_name = QString::fromStdString(triggers.controls[TRIGGER_COUNT + t]->ui_name);
    auto const button_name = QString::fromStdString(triggers.controls[t]->ui_name);

    const QRectF trigger_rect(0, 0, trigger_width, trigger_height);

    const QRectF analog_rect(0, 0, trigger_analog_width, trigger_height);

    // Unactivated analog text:
    p.setPen(TEXT_COLOR);
    p.drawText(analog_rect, Qt::AlignCenter, analog_name);

    const QRectF adj_analog_rect(0, 0, adj_analog * trigger_analog_width, trigger_height);

    // Trigger analog:
    p.setPen(Qt::NoPen);
    p.setBrush(RAW_INPUT_COLOR);
    p.drawEllipse(QPoint(raw_analog * trigger_analog_width, trigger_height - INPUT_DOT_RADIUS),
                  INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
    p.setBrush(ADJ_INPUT_COLOR);
    p.drawRect(adj_analog_rect);

    // Deadzone:
    p.setPen(DEADZONE_COLOR);
    p.setBrush(DEADZONE_BRUSH);
    p.drawRect(0, 0, trigger_analog_width * deadzone, trigger_height);

    // Threshold setting:
    const int threshold_x = trigger_analog_width * threshold;
    p.setPen(INPUT_SHAPE_PEN);
    p.drawLine(threshold_x, 0, threshold_x, trigger_height);

    const QRectF button_rect(trigger_analog_width, 0, trigger_button_width, trigger_height);

    // Trigger button:
    p.setPen(BBOX_PEN_COLOR);
    p.setBrush(trigger_button ? ADJ_INPUT_COLOR : BBOX_BRUSH_COLOR);
    p.drawRect(button_rect);

    // Bounding box outline:
    p.setPen(BBOX_PEN_COLOR);
    p.setBrush(Qt::NoBrush);
    p.drawRect(trigger_rect);

    // Button text:
    p.setPen(TEXT_COLOR);
    p.setPen(trigger_button ? TEXT_ALT_COLOR : TEXT_COLOR);
    p.drawText(button_rect, Qt::AlignCenter, button_name);

    // Activated analog text:
    p.setPen(TEXT_ALT_COLOR);
    p.setClipping(true);
    p.setClipRect(adj_analog_rect);
    p.drawText(analog_rect, Qt::AlignCenter, analog_name);
    p.setClipping(false);

    // Move down for next trigger:
    p.translate(0.0, trigger_height);
  }
}

void MappingIndicator::paintEvent(QPaintEvent*)
{
  switch (m_group->type)
  {
  case ControllerEmu::GroupType::Cursor:
    DrawCursor(*static_cast<ControllerEmu::Cursor*>(m_group));
    break;
  case ControllerEmu::GroupType::Stick:
  case ControllerEmu::GroupType::Tilt:
    DrawReshapableInput(*static_cast<ControllerEmu::ReshapableInput*>(m_group));
    break;
  case ControllerEmu::GroupType::MixedTriggers:
    DrawMixedTriggers();
    break;
  default:
    break;
  }
}
