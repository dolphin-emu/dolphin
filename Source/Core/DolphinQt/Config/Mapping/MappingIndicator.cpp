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
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/Device.h"

#include "DolphinQt/Settings.h"

// Color constants to keep things looking consistent:
// TODO: could we maybe query theme colors from Qt for the bounding box?
static const QColor BBOX_PEN_COLOR = Qt::darkGray;
static const QColor BBOX_BRUSH_COLOR = Qt::white;

static const QColor RAW_INPUT_COLOR = Qt::darkGray;
static const QColor ADJ_INPUT_COLOR = Qt::red;
static const QPen INPUT_SHAPE_PEN(RAW_INPUT_COLOR, 1.0, Qt::DashLine);

static const QColor DEADZONE_COLOR = Qt::darkGray;
static const QBrush DEADZONE_BRUSH(DEADZONE_COLOR, Qt::BDiagPattern);

static const QColor TEXT_COLOR = Qt::darkGray;
// Text color that is visible atop ADJ_INPUT_COLOR:
static const QColor TEXT_ALT_COLOR = Qt::white;

MappingIndicator::MappingIndicator(ControllerEmu::ControlGroup* group) : m_group(group)
{
  setMinimumHeight(128);

  switch (m_group->type)
  {
  case ControllerEmu::GroupType::Cursor:
    BindCursorControls(false);
    break;
  case ControllerEmu::GroupType::Stick:
    // Nothing needed:
    break;
  case ControllerEmu::GroupType::Tilt:
    BindCursorControls(true);
    break;
  case ControllerEmu::GroupType::MixedTriggers:
    // Nothing needed:
    break;
  default:
    break;
  }

  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, [this] { repaint(); });
  m_timer->start(1000 / 30);
}

void MappingIndicator::BindCursorControls(bool tilt)
{
  m_cursor_up = m_group->controls[0]->control_ref.get();
  m_cursor_down = m_group->controls[1]->control_ref.get();
  m_cursor_left = m_group->controls[2]->control_ref.get();
  m_cursor_right = m_group->controls[3]->control_ref.get();

  if (!tilt)
  {
    m_cursor_forward = m_group->controls[4]->control_ref.get();
    m_cursor_backward = m_group->controls[5]->control_ref.get();

    m_cursor_center = m_group->numeric_settings[0].get();
    m_cursor_width = m_group->numeric_settings[1].get();
    m_cursor_height = m_group->numeric_settings[2].get();
    m_cursor_deadzone = m_group->numeric_settings[3].get();
  }
  else
  {
    m_cursor_deadzone = m_group->numeric_settings[0].get();
  }
}

static ControlState PollControlState(ControlReference* ref)
{
  Settings::Instance().SetControllerStateNeeded(true);

  auto state = ref->State();

  Settings::Instance().SetControllerStateNeeded(false);

  if (state != 0)
    return state;
  else
    return 0;
}

void MappingIndicator::DrawCursor(bool tilt)
{
  float centerx = width() / 2., centery = height() / 2.;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  float width = 64, height = 64;
  float deadzone = m_cursor_deadzone->GetValue() * 48;

  if (!tilt)
  {
    float depth = centery - PollControlState(m_cursor_forward) * this->height() / 2.5 +
                  PollControlState(m_cursor_backward) * this->height() / 2.5;

    p.fillRect(0, depth, this->width(), 4, Qt::gray);

    width *= m_cursor_width->GetValue();
    height *= m_cursor_height->GetValue();
  }

  float curx = centerx - 4 - std::min(PollControlState(m_cursor_left), 0.5) * width +
               std::min(PollControlState(m_cursor_right), 0.5) * width,
        cury = centery - 4 - std::min(PollControlState(m_cursor_up), 0.5) * height +
               std::min(PollControlState(m_cursor_down), 0.5) * height;

  // Draw background
  p.setBrush(Qt::white);
  p.setPen(Qt::black);
  p.drawRect(centerx - (width / 2), centery - (height / 2), width, height);

  // Draw deadzone
  p.setBrush(Qt::lightGray);
  p.drawEllipse(centerx - (deadzone / 2), centery - (deadzone / 2), deadzone, deadzone);

  // Draw cursor
  p.fillRect(curx, cury, 8, 8, Qt::red);
}

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

void MappingIndicator::DrawStick()
{
  // Make the c-stick yellow:
  const bool is_c_stick = m_group->name == "C-Stick";
  const QColor gate_brush_color = is_c_stick ? Qt::yellow : Qt::lightGray;
  const QColor gate_pen_color = gate_brush_color.darker(125);

  auto& stick = *static_cast<ControllerEmu::AnalogStick*>(m_group);

  // TODO: This SetControllerStateNeeded interface leaks input into the game
  // We should probably hold the mutex for UI updates.
  Settings::Instance().SetControllerStateNeeded(true);
  const auto raw_coord = stick.GetState(false);
  const auto adj_coord = stick.GetState(true);
  Settings::Instance().SetControllerStateNeeded(false);

  // Bounding box size:
  const double scale = height() / 2.5;

  const float dot_radius = 2;

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
  p.drawEllipse(QPointF{raw_coord.x, raw_coord.y} * scale, dot_radius, dot_radius);

  // Adjusted stick position.
  if (adj_coord.x || adj_coord.y)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(ADJ_INPUT_COLOR);
    p.drawEllipse(QPointF{adj_coord.x, adj_coord.y} * scale, dot_radius, dot_radius);
  }
}

void MappingIndicator::DrawMixedTriggers()
{
  QPainter p(this);
  p.setRenderHint(QPainter::TextAntialiasing, true);

  auto& triggers = *static_cast<ControllerEmu::MixedTriggers*>(m_group);
  const ControlState threshold = triggers.numeric_settings[0]->GetValue();

  // MixedTriggers interface is a bit ugly:
  constexpr int TRIGGER_COUNT = 2;
  std::array<ControlState, TRIGGER_COUNT> analog_state;
  const std::array<u16, TRIGGER_COUNT> button_masks = {0x1, 0x2};
  u16 button_state = 0;

  Settings::Instance().SetControllerStateNeeded(true);
  triggers.GetState(&button_state, button_masks.data(), analog_state.data());
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
    const double trigger_analog = analog_state[t];
    const bool trigger_button = button_state & button_masks[t];
    auto const analog_name = QString::fromStdString(triggers.controls[TRIGGER_COUNT + t]->ui_name);
    auto const button_name = QString::fromStdString(triggers.controls[t]->ui_name);

    const QRectF trigger_rect(0, 0, trigger_width, trigger_height);

    const QRectF analog_rect(0, 0, trigger_analog_width, trigger_height);

    // Unactivated analog text:
    p.setPen(TEXT_COLOR);
    p.drawText(analog_rect, Qt::AlignCenter, analog_name);

    const QRectF activated_analog_rect(0, 0, trigger_analog * trigger_analog_width, trigger_height);

    // Trigger analog:
    p.setPen(Qt::NoPen);
    p.setBrush(ADJ_INPUT_COLOR);
    p.drawRect(activated_analog_rect);

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
    p.setClipRect(activated_analog_rect);
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
    DrawCursor(false);
    break;
  case ControllerEmu::GroupType::Tilt:
    DrawCursor(true);
    break;
  case ControllerEmu::GroupType::Stick:
    DrawStick();
    break;
  case ControllerEmu::GroupType::MixedTriggers:
    DrawMixedTriggers();
    break;
  default:
    break;
  }
}
