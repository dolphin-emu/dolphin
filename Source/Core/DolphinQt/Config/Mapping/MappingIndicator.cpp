// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/MappingIndicator.h"

#include <array>
#include <cmath>
#include <numbers>

#include <fmt/format.h>

#include <QAction>
#include <QDateTime>
#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>

#include "Common/MathUtil.h"

#include "Core/HW/WiimoteEmu/Camera.h"

#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/MappingCommon.h"

#include "DolphinQt/Config/Mapping/MappingWidget.h"
#include "DolphinQt/Config/Mapping/MappingWindow.h"
#include "DolphinQt/Settings.h"

namespace
{
const QColor STICK_GATE_COLOR = Qt::lightGray;
const QColor C_STICK_GATE_COLOR = Qt::yellow;
const QColor CURSOR_TV_COLOR = 0xaed6f1;
const QColor TILT_GATE_COLOR = 0xa2d9ce;
const QColor SWING_GATE_COLOR = 0xcea2d9;

constexpr int INPUT_DOT_RADIUS = 2;

constexpr int NORMAL_INDICATOR_WIDTH = 100;
constexpr int NORMAL_INDICATOR_HEIGHT = 100;
constexpr int NORMAL_INDICATOR_PADDING = 2;

// Per trigger.
constexpr int TRIGGER_INDICATOR_HEIGHT = 32;

QPen GetCosmeticPen(QPen pen)
{
  pen.setCosmetic(true);
  return pen;
}

QPen GetInputDotPen(QPen pen)
{
  pen.setWidth(INPUT_DOT_RADIUS * 2);
  pen.setCapStyle(Qt::PenCapStyle::RoundCap);
  return GetCosmeticPen(pen);
}

}  // namespace

QPen MappingIndicator::GetBBoxPen() const
{
  return QPen(palette().shadow().color(), 0);
}

QBrush MappingIndicator::GetBBoxBrush() const
{
  return palette().base();
}

QColor MappingIndicator::GetRawInputColor() const
{
  QColor color = palette().text().color();
  color.setAlphaF(0.5);
  return color;
}

QPen MappingIndicator::GetInputShapePen() const
{
  return QPen{GetRawInputColor(), 0.0, Qt::DashLine};
}

QColor MappingIndicator::GetAdjustedInputColor() const
{
  return Qt::red;
}

QColor MappingIndicator::GetCenterColor() const
{
  return Qt::blue;
}

QColor MappingIndicator::GetDeadZoneColor() const
{
  QColor color = Settings::Instance().IsThemeDark() ? Qt::white : Qt::black;
  color.setAlphaF(0.25);
  return color;
}

QPen MappingIndicator::GetDeadZonePen() const
{
  return QPen(GetDeadZoneColor(), 0);
}

QBrush MappingIndicator::GetDeadZoneBrush(QPainter& painter) const
{
  QBrush brush{GetDeadZoneColor(), Qt::FDiagPattern};
  brush.setTransform(painter.transform().inverted());
  return brush;
}

QColor MappingIndicator::GetTextColor() const
{
  return palette().text().color();
}

// Text color that is visible atop GetAdjustedInputColor():
QColor MappingIndicator::GetAltTextColor() const
{
  return palette().highlightedText().color();
}

void MappingIndicator::AdjustGateColor(QColor* color)
{
  if (Settings::Instance().IsThemeDark())
    color->setHsvF(color->hueF(), std::min(color->saturationF(), 0.5f), color->valueF() * 0.35f);
}

ButtonIndicator::ButtonIndicator(ControlReference* control_ref) : m_control_ref{control_ref}
{
  setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
}

QSize ButtonIndicator::sizeHint() const
{
  return QSize{INPUT_DOT_RADIUS + 2,
               QFontMetrics(font()).boundingRect(QStringLiteral("[")).height()};
}

void ButtonIndicator::Draw()
{
  QPainter p(this);
  p.setBrush(GetBBoxBrush());
  p.setPen(GetBBoxPen());
  p.drawRect(QRect{{0, 0}, size() - QSize{1, 1}});

  const auto input_value = std::clamp(m_control_ref->GetState<ControlState>(), 0.0, 1.0);
  const bool is_pressed = std::lround(input_value) != 0;
  QSizeF value_size = size() - QSizeF{2, 2};
  value_size.setHeight(value_size.height() * input_value);

  p.translate(0, height());
  p.scale(1, -1);

  p.setPen(Qt::NoPen);
  p.setBrush(is_pressed ? GetAdjustedInputColor() : GetRawInputColor());
  p.drawRect(QRectF{{1, 1}, value_size});
}

SquareIndicator::SquareIndicator()
{
  // Additional pixel for border.
  setFixedWidth(NORMAL_INDICATOR_WIDTH + (NORMAL_INDICATOR_PADDING + 1) * 2);
  setFixedHeight(NORMAL_INDICATOR_HEIGHT + (NORMAL_INDICATOR_PADDING + 1) * 2);
}

MixedTriggersIndicator::MixedTriggersIndicator(ControllerEmu::MixedTriggers& group) : m_group(group)
{
  setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Ignored);
  setFixedHeight(TRIGGER_INDICATOR_HEIGHT * int(group.GetTriggerCount()) + 1);
}

namespace
{
constexpr float SPHERE_SIZE = 0.7f;
constexpr float SPHERE_INDICATOR_DIST = 0.85f;
constexpr int SPHERE_POINT_COUNT = 200;

// Constructs a polygon by querying a radius at varying angles:
template <typename F>
QPolygonF GetPolygonFromRadiusGetter(F&& radius_getter)
{
  // A multiple of 8 (octagon) and enough points to be visibly pleasing:
  constexpr int shape_point_count = 32;
  QPolygonF shape{shape_point_count};

  int p = 0;
  for (auto& point : shape)
  {
    const double angle = MathUtil::TAU * p / shape.size();
    const double radius = radius_getter(angle);

    point = {std::cos(angle) * radius, std::sin(angle) * radius};
    ++p;
  }

  return shape;
}

// Constructs a polygon by querying a radius at varying angles:
template <typename F>
QPolygonF GetPolygonSegmentFromRadiusGetter(F&& radius_getter, double direction,
                                            double segment_size, double segment_depth)
{
  constexpr int shape_point_count = 6;
  QPolygonF shape{shape_point_count};

  // We subtract from the provided direction angle so it's better
  // to add Tau here to prevent a negative value instead of
  // expecting the function call to be aware of this internal logic
  const double center_angle = direction + MathUtil::TAU;
  const double center_radius_outer = radius_getter(center_angle);
  const double center_radius_inner = center_radius_outer - segment_depth;

  const double lower_angle = center_angle - segment_size / 2;
  const double lower_radius_outer = radius_getter(lower_angle);
  const double lower_radius_inner = lower_radius_outer - segment_depth;

  const double upper_angle = center_angle + segment_size / 2;
  const double upper_radius_outer = radius_getter(upper_angle);
  const double upper_radius_inner = upper_radius_outer - segment_depth;

  shape[0] = {std::cos(lower_angle) * (lower_radius_inner),
              std::sin(lower_angle) * (lower_radius_inner)};
  shape[1] = {std::cos(center_angle) * (center_radius_inner),
              std::sin(center_angle) * (center_radius_inner)};
  shape[2] = {std::cos(upper_angle) * (upper_radius_inner),
              std::sin(upper_angle) * (upper_radius_inner)};
  shape[3] = {std::cos(upper_angle) * upper_radius_outer,
              std::sin(upper_angle) * upper_radius_outer};
  shape[4] = {std::cos(center_angle) * center_radius_outer,
              std::sin(center_angle) * center_radius_outer};
  shape[5] = {std::cos(lower_angle) * lower_radius_outer,
              std::sin(lower_angle) * lower_radius_outer};

  return shape;
}

// Used to test for a miscalibrated stick so the user can be informed.
bool IsPointOutsideCalibration(Common::DVec2 point, ControllerEmu::ReshapableInput& input)
{
  const auto center = input.GetCenter();
  const double current_radius = (point - center).Length();
  const double input_radius = input.GetInputRadiusAtAngle(
      std::atan2(point.y - center.y, point.x - center.x) + MathUtil::TAU);

  constexpr double ALLOWED_ERROR = 1.3;

  return current_radius > input_radius * ALLOWED_ERROR;
}

void DrawVirtualNotches(QPainter& p, ControllerEmu::ReshapableInput& stick, QColor notch_color)
{
  const double segment_size = stick.GetVirtualNotchSize();
  if (segment_size <= 0.0)
    return;

  p.setBrush(notch_color);
  for (int i = 0; i < 8; ++i)
  {
    const double segment_depth = 1.0 - ControllerEmu::MINIMUM_NOTCH_DISTANCE;
    const double segment_gap = MathUtil::TAU / 8.0;
    const double direction = segment_gap * i;
    p.drawPolygon(GetPolygonSegmentFromRadiusGetter(
        [&stick](double ang) { return stick.GetGateRadiusAtAngle(ang); }, direction, segment_size,
        segment_depth));
  }
}

template <typename F>
void GenerateFibonacciSphere(int point_count, F&& callback)
{
  const float golden_angle = MathUtil::PI * (3.f - std::sqrt(5.f));

  for (int i = 0; i != point_count; ++i)
  {
    const float z = (1.f / point_count - 1.f) + (2.f / point_count) * i;
    const float r = std::sqrt(1.f - z * z);
    const float x = std::cos(golden_angle * i) * r;
    const float y = std::sin(golden_angle * i) * r;

    callback(Common::Vec3{x, y, z});
  }
}

// Draws an analog stick pushed to the right by the provided amount.
void DrawPushedStick(QPainter& p, ReshapableInputIndicator& indicator, double value)
{
  auto stick_color = indicator.GetGateBrushColor();
  indicator.AdjustGateColor(&stick_color);
  const auto stick_pen_color = stick_color.darker(125);
  p.setPen(QPen{stick_pen_color, 0});
  p.setBrush(stick_color);
  constexpr float circle_radius = 0.65f;
  p.drawEllipse(QPointF{value * 0.35f, 0.f}, circle_radius, circle_radius);

  p.setPen(QPen{indicator.GetRawInputColor(), 0});
  p.setBrush(Qt::NoBrush);
  constexpr float alt_circle_radius = 0.45f;
  p.drawEllipse(QPointF{value * 0.45f, 0.f}, alt_circle_radius, alt_circle_radius);
}

}  // namespace

void MappingIndicator::paintEvent(QPaintEvent*)
{
  constexpr float max_elapsed_seconds = 0.1f;

  const auto now = Clock::now();
  const float elapsed_seconds = std::chrono::duration_cast<DT_s>(now - m_last_update).count();
  m_last_update = now;

  const auto lock = ControllerEmu::EmulatedController::GetStateLock();
  Update(std::min(elapsed_seconds, max_elapsed_seconds));
  Draw();
}

QColor CursorIndicator::GetGateBrushColor() const
{
  return CURSOR_TV_COLOR;
}

void CursorIndicator::Draw()
{
  const auto adj_coord = m_cursor_group.GetState(true);

  DrawReshapableInput(m_cursor_group,
                      adj_coord.IsVisible() ?
                          std::make_optional(Common::DVec2(adj_coord.x, adj_coord.y)) :
                          std::nullopt);
}

qreal SquareIndicator::GetContentsScale() const
{
  return (NORMAL_INDICATOR_WIDTH - 1.0) / 2;
}

void SquareIndicator::DrawBoundingBox(QPainter& p)
{
  p.setBrush(GetBBoxBrush());
  p.setPen(GetBBoxPen());
  p.drawRect(QRectF{NORMAL_INDICATOR_PADDING + 0.5, NORMAL_INDICATOR_PADDING + 0.5,
                    NORMAL_INDICATOR_WIDTH + 1.0, NORMAL_INDICATOR_HEIGHT + 1.0});
}

void SquareIndicator::TransformPainter(QPainter& p)
{
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  p.translate(width() / 2.0, height() / 2.0);
  const auto scale = GetContentsScale();
  p.scale(scale, scale);
}

void ReshapableInputIndicator::DrawReshapableInput(
    ControllerEmu::ReshapableInput& stick,
    std::optional<ControllerEmu::ReshapableInput::ReshapeData> adj_coord)
{
  QPainter p(this);
  DrawBoundingBox(p);
  TransformPainter(p);

  // UI y-axis is opposite that of stick.
  p.scale(1.0, -1.0);

  const auto raw_coord = stick.GetReshapableState(false);

  UpdateCalibrationWidget(raw_coord);

  if (IsCalibrating())
  {
    m_calibration_widget->Draw(p, raw_coord);
    return;
  }

  DrawUnderGate(p);

  auto gate_brush_color = GetGateBrushColor();
  auto gate_pen_color = gate_brush_color.darker(125);

  AdjustGateColor(&gate_brush_color);
  AdjustGateColor(&gate_pen_color);

  // Input gate. (i.e. the octagon shape)
  p.setPen(QPen(gate_pen_color, 0));
  p.setBrush(gate_brush_color);
  p.drawPolygon(
      GetPolygonFromRadiusGetter([&stick](double ang) { return stick.GetGateRadiusAtAngle(ang); }));

  DrawVirtualNotches(p, stick, gate_pen_color);

  const auto center = stick.GetCenter();

  p.save();
  p.translate(center.x, center.y);

  // Deadzone.
  p.setPen(GetDeadZonePen());
  p.setBrush(GetDeadZoneBrush(p));
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&stick](double ang) { return stick.GetDeadzoneRadiusAtAngle(ang); }));

  // Input shape.
  p.setPen(GetInputShapePen());
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&stick](double ang) { return stick.GetInputRadiusAtAngle(ang); }));

  // Center.
  if (center.x || center.y)
  {
    p.setPen(GetInputDotPen(GetCenterColor()));
    p.drawPoint(QPointF{});
  }

  p.restore();

  // Raw stick position.
  p.setPen(GetInputDotPen(GetRawInputColor()));
  p.drawPoint(QPointF{raw_coord.x, raw_coord.y});

  // Adjusted stick position.
  if (adj_coord)
  {
    p.setPen(GetInputDotPen(GetAdjustedInputColor()));
    p.drawPoint(QPointF{adj_coord->x, adj_coord->y});
  }
}

QColor AnalogStickIndicator::GetGateBrushColor() const
{
  // Some hacks for pretty colors:
  const bool is_c_stick = m_group.name == "C-Stick";

  return is_c_stick ? C_STICK_GATE_COLOR : STICK_GATE_COLOR;
}

void AnalogStickIndicator::Draw()
{
  const auto adj_coord = m_group.GetReshapableState(true);

  DrawReshapableInput(m_group,
                      (adj_coord.x || adj_coord.y) ? std::make_optional(adj_coord) : std::nullopt);
}

void TiltIndicator::Update(float elapsed_seconds)
{
  ReshapableInputIndicator::Update(elapsed_seconds);
  WiimoteEmu::EmulateTilt(&m_motion_state, &m_group, elapsed_seconds);
}

QColor TiltIndicator::GetGateBrushColor() const
{
  return TILT_GATE_COLOR;
}

void TiltIndicator::Draw()
{
  auto adj_coord = Common::DVec2{-m_motion_state.angle.y, m_motion_state.angle.x} / MathUtil::PI;

  // Angle values after dividing by pi.
  constexpr auto norm_180_deg = 1;
  constexpr auto norm_360_deg = 2;

  // Angle may extend beyond 180 degrees when wrapping around.
  // Apply modulo to draw within the indicator.
  // Scale down the value a bit so +1 does not become -1.
  adj_coord *= 0.9999f;
  adj_coord.x = std::fmod(adj_coord.x + norm_360_deg + norm_180_deg, norm_360_deg) - norm_180_deg;
  adj_coord.y = std::fmod(adj_coord.y + norm_360_deg + norm_180_deg, norm_360_deg) - norm_180_deg;

  DrawReshapableInput(m_group,
                      (adj_coord.x || adj_coord.y) ? std::make_optional(adj_coord) : std::nullopt);
}

void MixedTriggersIndicator::Draw()
{
  QPainter p(this);
  p.setRenderHint(QPainter::TextAntialiasing, true);

  const auto& triggers = m_group;
  const ControlState threshold = triggers.GetThreshold();
  const ControlState deadzone = triggers.GetDeadzone();

  // MixedTriggers interface is a bit ugly:
  constexpr int TRIGGER_COUNT = 2;
  std::array<ControlState, TRIGGER_COUNT> raw_analog_state;
  std::array<ControlState, TRIGGER_COUNT> adj_analog_state;
  const std::array<u16, TRIGGER_COUNT> button_masks = {0x1, 0x2};
  u16 button_state = 0;

  triggers.GetState(&button_state, button_masks.data(), raw_analog_state.data(), false);
  triggers.GetState(&button_state, button_masks.data(), adj_analog_state.data(), true);

  // Rectangle sizes:
  const int trigger_height = TRIGGER_INDICATOR_HEIGHT;
  const int trigger_width = width() - 1;
  const int trigger_button_width = trigger_height;
  const int trigger_analog_width = trigger_width - trigger_button_width;

  // Bounding box background:
  p.setPen(Qt::NoPen);
  p.setBrush(GetBBoxBrush());
  p.drawRect(QRectF(0.5, 0.5, trigger_width, trigger_height * TRIGGER_COUNT));

  for (int t = 0; t != TRIGGER_COUNT; ++t)
  {
    const double raw_analog = raw_analog_state[t];
    const double adj_analog = adj_analog_state[t];
    const bool trigger_button = button_state & button_masks[t];
    auto const analog_name = QString::fromStdString(triggers.controls[TRIGGER_COUNT + t]->ui_name);
    auto const button_name = QString::fromStdString(triggers.controls[t]->ui_name);

    const QRectF trigger_rect(0.5, 0.5, trigger_width, trigger_height);

    const QRectF analog_rect(0.5, 0.5, trigger_analog_width, trigger_height);

    // Unactivated analog text:
    p.setPen(GetTextColor());
    p.drawText(analog_rect, Qt::AlignCenter, analog_name);

    const QRectF adj_analog_rect(0.5, 0.5, adj_analog * trigger_analog_width, trigger_height);

    // Trigger analog:
    p.setPen(Qt::NoPen);
    p.setBrush(GetAdjustedInputColor());
    p.drawRect(adj_analog_rect);

    p.setPen(GetInputDotPen(GetRawInputColor()));
    p.drawPoint(QPoint(raw_analog * trigger_analog_width, trigger_height - INPUT_DOT_RADIUS));

    // Deadzone:
    p.setPen(GetDeadZonePen());
    p.setBrush(GetDeadZoneBrush(p));
    p.drawRect(QRectF(1.5, 1.5, trigger_analog_width * deadzone, trigger_height - 2));

    // Threshold setting:
    const int threshold_x = trigger_analog_width * threshold;
    p.setPen(GetInputShapePen());
    p.drawLine(threshold_x, 0, threshold_x, trigger_height);

    const QRectF button_rect(trigger_analog_width + 0.5, 0.5, trigger_button_width, trigger_height);

    // Trigger button:
    p.setPen(GetBBoxPen());
    p.setBrush(trigger_button ? GetAdjustedInputColor() : GetBBoxBrush());
    p.drawRect(button_rect);

    // Bounding box outline:
    p.setPen(GetBBoxPen());
    p.setBrush(Qt::NoBrush);
    p.drawRect(trigger_rect);

    // Button text:
    p.setPen(GetTextColor());
    p.setPen(trigger_button ? GetAltTextColor() : GetTextColor());
    p.drawText(button_rect, Qt::AlignCenter, button_name);

    // Activated analog text:
    p.setPen(GetAltTextColor());
    p.setClipping(true);
    p.setClipRect(adj_analog_rect);
    p.drawText(analog_rect, Qt::AlignCenter, analog_name);
    p.setClipping(false);

    // Move down for next trigger:
    p.translate(0.0, trigger_height);
  }
}

void SwingIndicator::DrawUnderGate(QPainter& p)
{
  auto& force = m_swing_group;

  // Deadzone for Z (forward/backward):
  const double deadzone = force.GetDeadzonePercentage();
  if (deadzone > 0.0)
  {
    p.setPen(GetDeadZonePen());
    p.setBrush(GetDeadZoneBrush(p));
    p.drawRect(QRectF(-1, -deadzone, 2, deadzone * 2));
  }

  // Raw Z:
  const auto raw_coord = force.GetState(false);
  p.setPen(GetCosmeticPen(QPen(GetRawInputColor(), INPUT_DOT_RADIUS)));
  p.drawLine(QLineF(-1, raw_coord.z, 1, raw_coord.z));

  // Adjusted Z:
  const auto& adj_coord = m_motion_state.position;
  const auto curve_point =
      std::max(std::abs(m_motion_state.angle.x), std::abs(m_motion_state.angle.z)) / MathUtil::TAU;
  if (adj_coord.y || curve_point)
  {
    // Show off the angle somewhat with a curved line.
    QPainterPath path;
    path.moveTo(-1.0, (adj_coord.y + curve_point) * -1);
    path.quadTo({0, (adj_coord.y - curve_point) * -1}, {1, (adj_coord.y + curve_point) * -1});

    p.setBrush(Qt::NoBrush);
    p.setPen(GetCosmeticPen(QPen(GetAdjustedInputColor(), INPUT_DOT_RADIUS)));
    p.drawPath(path);
  }
}

void SwingIndicator::Update(float elapsed_seconds)
{
  ReshapableInputIndicator::Update(elapsed_seconds);
  WiimoteEmu::EmulateSwing(&m_motion_state, &m_swing_group, elapsed_seconds);
}

QColor SwingIndicator::GetGateBrushColor() const
{
  return SWING_GATE_COLOR;
}

void SwingIndicator::Draw()
{
  DrawReshapableInput(m_swing_group,
                      Common::DVec2{-m_motion_state.position.x, m_motion_state.position.z});
}

void ShakeMappingIndicator::Update(float elapsed_seconds)
{
  WiimoteEmu::EmulateShake(&m_motion_state, &m_shake_group, elapsed_seconds);

  for (auto& sample : m_position_samples)
    sample.age += elapsed_seconds;

  m_position_samples.erase(
      std::ranges::find_if(m_position_samples,
                           [](const ShakeSample& sample) { return sample.age > 1.f; }),
      m_position_samples.end());

  constexpr float MAX_DISTANCE = 0.5f;

  m_position_samples.push_front(ShakeSample{m_motion_state.position / MAX_DISTANCE});

  const bool any_non_zero_samples = std::ranges::any_of(
      m_position_samples, [](const ShakeSample& s) { return s.state.LengthSquared() != 0.0; });

  // Only start moving the line if there's non-zero data.
  if (m_grid_line_position || any_non_zero_samples)
  {
    m_grid_line_position += elapsed_seconds;

    if (m_grid_line_position > 1.f)
    {
      if (any_non_zero_samples)
        m_grid_line_position = std::fmod(m_grid_line_position, 1.f);
      else
        m_grid_line_position = 0;
    }
  }
}

void ShakeMappingIndicator::Draw()
{
  QPainter p(this);
  DrawBoundingBox(p);
  TransformPainter(p);

  // UI y-axis is opposite that of acceleration Z.
  p.scale(1.0, -1.0);

  // Deadzone.
  const double deadzone = m_shake_group.GetDeadzone();
  if (deadzone > 0.0)
  {
    p.setPen(GetDeadZonePen());
    p.setBrush(GetDeadZoneBrush(p));
    p.drawRect(QRectF(-1, 0, 2, deadzone));
  }

  // Raw input.
  const auto raw_coord = m_shake_group.GetState(false);
  p.setPen(GetInputDotPen(GetRawInputColor()));
  for (std::size_t c = 0; c != raw_coord.data.size(); ++c)
  {
    p.drawPoint(QPointF{-0.5 + c * 0.5, raw_coord.data[c]});
  }

  const double grid_line_x = 1.0 - m_grid_line_position * 2.0;
  p.setPen(QPen(GetRawInputColor(), 0));
  p.drawLine(QPointF{grid_line_x, -1.0}, QPointF{grid_line_x, 1.0});

  // Position history.
  const QColor component_colors[] = {Qt::blue, Qt::green, Qt::red};
  p.setBrush(Qt::NoBrush);
  for (std::size_t c = 0; c != raw_coord.data.size(); ++c)
  {
    QPolygonF polyline;

    for (auto& sample : m_position_samples)
      polyline.append(QPointF{1.0 - sample.age * 2.0, sample.state.data[c]});

    p.setPen(QPen(component_colors[c], 0));
    p.drawPolyline(polyline);
  }
}

void AccelerometerMappingIndicator::Draw()
{
  const auto accel_state = m_accel_group.GetState();
  const auto state = accel_state.value_or(Common::Vec3{});

  QPainter p(this);
  p.setRenderHint(QPainter::TextAntialiasing, true);
  DrawBoundingBox(p);
  TransformPainter(p);

  // UI axes are opposite that of Wii remote accelerometer.
  p.scale(-1.0, -1.0);

  const auto rotation = WiimoteEmu::GetRotationFromAcceleration(state);

  // Draw sphere.
  p.setPen(GetCosmeticPen(QPen(GetRawInputColor(), 0.5)));

  GenerateFibonacciSphere(SPHERE_POINT_COUNT, [&](const Common::Vec3& point) {
    const auto pt = rotation * point;

    if (pt.y > 0)
      p.drawPoint(QPointF(pt.x, pt.z) * SPHERE_SIZE);
  });

  // Sphere outline.
  p.setPen(QPen(GetRawInputColor(), 0));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF{}, SPHERE_SIZE, SPHERE_SIZE);

  p.setPen(Qt::NoPen);

  // Red dot.
  const auto point = rotation * Common::Vec3{0, 0, SPHERE_INDICATOR_DIST};
  if (point.y > 0 || Common::Vec2(point.x, point.z).Length() > SPHERE_SIZE)
  {
    p.setPen(GetInputDotPen(GetAdjustedInputColor()));
    p.drawPoint(QPointF(point.x, point.z));
  }

  // Blue dot.
  const auto point2 = -point;
  if (point2.y > 0 || Common::Vec2(point2.x, point2.z).Length() > SPHERE_SIZE)
  {
    p.setPen(GetInputDotPen(GetCenterColor()));
    p.drawPoint(QPointF(point2.x, point2.z));
  }

  p.setBrush(Qt::NoBrush);

  p.resetTransform();
  p.translate(width() / 2.0, height() / 2.0);

  // Red dot upright target.
  p.setPen(GetAdjustedInputColor());
  p.drawEllipse(QPointF{0, -SPHERE_INDICATOR_DIST} * GetContentsScale(), INPUT_DOT_RADIUS,
                INPUT_DOT_RADIUS);

  // Blue dot target.
  p.setPen(GetCenterColor());
  p.drawEllipse(QPointF{0, SPHERE_INDICATOR_DIST} * GetContentsScale(), INPUT_DOT_RADIUS,
                INPUT_DOT_RADIUS);

  // Only draw g-force text if acceleration data is present.
  if (!accel_state.has_value())
    return;

  // G-force text:
  p.setPen(GetTextColor());
  p.drawText(QRect(0, 0, NORMAL_INDICATOR_WIDTH / 2 - 2, NORMAL_INDICATOR_HEIGHT / 2 - 1),
             Qt::AlignBottom | Qt::AlignRight,
             QString::fromStdString(
                 // i18n: "g" is the symbol for "gravitational force equivalent" (g-force).
                 fmt::format("{:.2f} g", state.Length() / WiimoteEmu::GRAVITY_ACCELERATION)));
}

void GyroMappingIndicator::Update(float elapsed_seconds)
{
  const auto gyro_state = m_gyro_group.GetState();
  const auto angular_velocity = gyro_state.value_or(Common::Vec3{});
  m_state *= WiimoteEmu::GetRotationFromGyroscope(angular_velocity * Common::Vec3(-1, +1, -1) *
                                                  elapsed_seconds);
  m_state = m_state.Normalized();

  // Reset orientation when stable for a bit:
  constexpr float STABLE_RESET_SECONDS = 1.f;
  // Consider device stable when data (with deadzone applied) is zero.
  const bool is_stable = !angular_velocity.LengthSquared();

  if (!is_stable)
    m_stable_time = 0;
  else if (m_stable_time < STABLE_RESET_SECONDS)
    m_stable_time += elapsed_seconds;

  if (m_stable_time >= STABLE_RESET_SECONDS)
    m_state = Common::Quaternion::Identity();
}

void GyroMappingIndicator::Draw()
{
  const auto gyro_state = m_gyro_group.GetState();
  const auto raw_gyro_state = m_gyro_group.GetRawState();
  const auto angular_velocity = gyro_state.value_or(Common::Vec3{});
  const auto jitter = raw_gyro_state - m_previous_velocity;
  m_previous_velocity = raw_gyro_state;

  // Consider device stable when data (with deadzone applied) is zero.
  const bool is_stable = !angular_velocity.LengthSquared();

  // Use an empty rotation matrix if gyroscope data is not present.
  const auto rotation =
      (gyro_state.has_value() ? Common::Matrix33::FromQuaternion(m_state) : Common::Matrix33{});

  QPainter p(this);
  DrawBoundingBox(p);
  TransformPainter(p);

  // Deadzone.
  if (const auto deadzone_value = m_gyro_group.GetDeadzone(); deadzone_value)
  {
    static constexpr auto DEADZONE_DRAW_SIZE = 1 - SPHERE_SIZE;
    static constexpr auto DEADZONE_DRAW_BOTTOM = 1;

    p.setPen(GetDeadZonePen());
    p.setBrush(GetDeadZoneBrush(p));
    p.drawRect(QRectF{-1, DEADZONE_DRAW_BOTTOM, 2, -DEADZONE_DRAW_SIZE});

    if (gyro_state.has_value())
    {
      const auto max_jitter =
          std::max({std::abs(jitter.x), std::abs(jitter.y), std::abs(jitter.z)});
      const auto jitter_line_y =
          std::min(max_jitter / deadzone_value * DEADZONE_DRAW_SIZE - DEADZONE_DRAW_BOTTOM, 1.0);
      p.setPen(GetCosmeticPen(QPen(GetRawInputColor(), INPUT_DOT_RADIUS)));
      p.drawLine(QLineF(-1.0, jitter_line_y * -1.0, 1.0, jitter_line_y * -1.0));

      // Sphere background.
      p.setPen(Qt::NoPen);
      p.setBrush(GetBBoxBrush());
      p.drawEllipse(QPointF{}, SPHERE_SIZE, SPHERE_SIZE);
    }
  }

  // Sphere dots.
  p.setPen(GetCosmeticPen(QPen(GetRawInputColor(), 0.5)));

  GenerateFibonacciSphere(SPHERE_POINT_COUNT, [&](const Common::Vec3& point) {
    const auto pt = rotation * point;

    if (pt.y > 0)
      p.drawPoint(QPointF(pt.x, pt.z) * SPHERE_SIZE);
  });

  // Sphere outline.
  const auto outline_color =
      is_stable ? (m_gyro_group.IsCalibrating() ? GetCenterColor() : GetRawInputColor()) :
                  GetAdjustedInputColor();
  p.setPen(QPen(outline_color, 0));
  p.setBrush(Qt::NoBrush);
  p.drawEllipse(QPointF{}, SPHERE_SIZE, SPHERE_SIZE);

  p.setPen(Qt::NoPen);

  // Red dot.
  const auto point = rotation * Common::Vec3{0, 0, -SPHERE_INDICATOR_DIST};
  if (point.y > 0 || Common::Vec2(point.x, point.z).Length() > SPHERE_SIZE)
  {
    p.setPen(GetInputDotPen(GetAdjustedInputColor()));
    p.drawPoint(QPointF(point.x, point.z));
  }
  // Blue dot.
  const auto point2 = rotation * Common::Vec3{0, SPHERE_INDICATOR_DIST, 0};
  if (point2.y > 0 || Common::Vec2(point2.x, point2.z).Length() > SPHERE_SIZE)
  {
    p.setPen(GetInputDotPen(GetCenterColor()));
    p.drawPoint(QPointF(point2.x, point2.z));
  }

  p.setBrush(Qt::NoBrush);

  p.resetTransform();
  p.translate(width() / 2.0, height() / 2.0);

  // Red dot upright target.
  p.setPen(GetAdjustedInputColor());
  p.drawEllipse(QPointF{0, -SPHERE_INDICATOR_DIST} * GetContentsScale(), INPUT_DOT_RADIUS,
                INPUT_DOT_RADIUS);

  // Blue dot target.
  p.setPen(GetCenterColor());
  p.drawEllipse(QPointF{}, INPUT_DOT_RADIUS, INPUT_DOT_RADIUS);
}

void IRPassthroughMappingIndicator::Draw()
{
  QPainter p(this);
  DrawBoundingBox(p);
  TransformPainter(p);

  p.scale(1.0, -1.0);

  auto pen =
      GetInputDotPen(m_ir_group.enabled.GetValue() ? GetAdjustedInputColor() : GetRawInputColor());

  for (std::size_t i = 0; i != WiimoteEmu::CameraLogic::NUM_POINTS; ++i)
  {
    const auto size = m_ir_group.GetObjectSize(i);

    const bool is_visible = size > 0;
    if (!is_visible)
      continue;

    const auto point =
        (QPointF{m_ir_group.GetObjectPositionX(i), m_ir_group.GetObjectPositionY(i)} -
         QPointF{0.5, 0.5}) *
        2.0;

    pen.setWidth(size * NORMAL_INDICATOR_WIDTH / 2);
    p.setPen(pen);
    p.drawPoint(point);
  }
}

void CalibrationWidget::Draw(QPainter& p, Common::DVec2 point)
{
  DrawInProgressMapping(p);
  DrawInProgressCalibration(p, point);
}

double CalibrationWidget::GetAnimationElapsedSeconds() const
{
  return DT_s{Clock::now() - m_animation_start_time}.count();
}

void CalibrationWidget::RestartAnimation()
{
  m_animation_start_time = Clock::now();
}

void CalibrationWidget::DrawInProgressMapping(QPainter& p)
{
  if (!IsMapping())
    return;

  p.rotate(qRadiansToDegrees(m_mapper->GetCurrentAngle()));

  const auto ping_pong = 1 - std::abs(1 - (2 * std::fmod(GetAnimationElapsedSeconds(), 1)));

  // Stick.
  DrawPushedStick(p, m_indicator,
                  QEasingCurve(QEasingCurve::OutBounce).valueForProgress(ping_pong));

  // Arrow.
  p.save();
  const auto triangle_x =
      (QEasingCurve(QEasingCurve::InOutQuart).valueForProgress(ping_pong) * 0.3) + 0.1;
  p.translate(triangle_x, 0.0);

  // An equilateral triangle.
  constexpr auto triangle_h = 0.2f;
  constexpr auto triangle_w_2 = triangle_h / std::numbers::sqrt3_v<float>;

  p.setPen(Qt::NoPen);
  p.setBrush(m_indicator.GetRawInputColor());
  p.drawPolygon(QPolygonF{{triangle_h, 0.f}, {0.f, -triangle_w_2}, {0.f, +triangle_w_2}});

  p.restore();
}

void CalibrationWidget::DrawInProgressCalibration(QPainter& p, Common::DVec2 point)
{
  if (!IsCalibrating())
    return;

  const auto elapsed_seconds = GetAnimationElapsedSeconds();

  const auto stop_spinning_amount =
      std::max(DT_s{m_stop_spinning_time - Clock::now()} / STOP_SPINNING_DURATION, 0.0);

  const auto stick_pushed_amount =
      QEasingCurve(QEasingCurve::OutCirc).valueForProgress(std::min(elapsed_seconds * 2, 1.0)) *
      stop_spinning_amount;

  // Clockwise spinning stick starting from center.
  p.save();
  p.rotate(elapsed_seconds * -360.0);
  DrawPushedStick(p, m_indicator, -stick_pushed_amount);
  p.restore();

  const auto center = m_calibrator->GetCenter();
  p.save();
  p.translate(center.x, center.y);

  // Input shape.
  p.setPen(m_indicator.GetInputShapePen());
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [this](double angle) { return m_calibrator->GetCalibrationRadiusAtAngle(angle); }));

  // Calibrated center.
  if (center.x || center.y)
  {
    p.setPen(GetInputDotPen(m_indicator.GetCenterColor()));
    p.drawPoint(QPointF{});
  }
  p.restore();

  // Show the red dot only if the input is at least halfway pressed.
  // The cool spinning stick is otherwise uglified by the red dot always being shown.
  if (Common::DVec2{point.x, point.y}.LengthSquared() > (0.5 * 0.5))
  {
    p.setPen(GetInputDotPen(m_indicator.GetAdjustedInputColor()));
    p.drawPoint(QPointF{point.x, point.y});
  }
}

void ReshapableInputIndicator::UpdateCalibrationWidget(Common::DVec2 point)
{
  if (m_calibration_widget != nullptr)
    m_calibration_widget->Update(point);
}

bool ReshapableInputIndicator::IsCalibrating() const
{
  return m_calibration_widget != nullptr && m_calibration_widget->IsActive();
}

void ReshapableInputIndicator::SetCalibrationWidget(CalibrationWidget* widget)
{
  m_calibration_widget = widget;
}

CalibrationWidget::CalibrationWidget(MappingWidget& mapping_widget,
                                     ControllerEmu::ReshapableInput& input,
                                     ReshapableInputIndicator& indicator)
    : m_mapping_widget(mapping_widget), m_input(input), m_indicator(indicator)
{
  connect(mapping_widget.GetParent(), &MappingWindow::CancelMapping, this,
          &CalibrationWidget::ResetActions);
  connect(mapping_widget.GetParent(), &MappingWindow::ConfigChanged, this,
          &CalibrationWidget::ResetActions);

  m_indicator.SetCalibrationWidget(this);

  // Make it more apparent that this is a menu with more options.
  setPopupMode(ToolButtonPopupMode::MenuButtonPopup);

  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

  ResetActions();
}

void CalibrationWidget::DeleteAllActions()
{
  for (auto* action : actions())
    delete action;
}

void CalibrationWidget::ResetActions()
{
  m_calibrator.reset();
  m_mapper.reset();

  // i18n: A button to start the process of game controller analog stick mapping and calibration.
  auto* const map_and_calibrate_action = new QAction(tr("Map and Calibrate"), this);

  // i18n: A button to start the process of game controller analog stick calibration.
  auto* const calibrate_action = new QAction(tr("Calibrate"), this);

  // i18n: A button to calibrate the center and extremities of a game controller analog stick.
  auto* const center_action = new QAction(tr("Center and Calibrate"), this);

  // i18n: A button to reset game controller analog stick calibration.
  auto* const reset_action = new QAction(tr("Reset Calibration"), this);

  connect(map_and_calibrate_action, &QAction::triggered, this,
          &CalibrationWidget::StartMappingAndCalibration);
  connect(calibrate_action, &QAction::triggered, this, [this]() { StartCalibration(); });
  connect(center_action, &QAction::triggered, this, [this]() { StartCalibration(std::nullopt); });
  connect(reset_action, &QAction::triggered, this, [this]() {
    const auto lock = m_mapping_widget.GetController()->GetStateLock();
    m_input.SetCalibrationToDefault();
    m_input.SetCenter({});
  });

  DeleteAllActions();

  addAction(map_and_calibrate_action);
  addAction(calibrate_action);
  addAction(center_action);
  addAction(reset_action);

  setDefaultAction(map_and_calibrate_action);
}

void CalibrationWidget::StartMappingAndCalibration()
{
  RestartAnimation();

  // i18n: A button to stop a game controller button mapping process.
  auto* const cancel_action = new QAction(tr("Cancel Mapping"), this);
  connect(cancel_action, &QAction::triggered, this, &CalibrationWidget::ResetActions);

  DeleteAllActions();

  addAction(cancel_action);
  setDefaultAction(cancel_action);

  auto* const window = m_mapping_widget.GetParent();
  const auto& default_device = window->GetController()->GetDefaultDevice();

  std::vector device_strings{default_device.ToString()};
  if (window->IsCreateOtherDeviceMappingsEnabled())
    device_strings = g_controller_interface.GetAllDeviceStrings();

  const auto lock = window->GetController()->GetStateLock();
  m_mapper = std::make_unique<ciface::MappingCommon::ReshapableInputMapper>(g_controller_interface,
                                                                            device_strings);
}

void CalibrationWidget::StartCalibration(std::optional<Common::DVec2> center)
{
  RestartAnimation();
  m_calibrator = std::make_unique<ciface::MappingCommon::CalibrationBuilder>(center);

  // i18n: A button to abort a game controller calibration process.
  auto* const cancel_action = new QAction(tr("Cancel Calibration"), this);
  connect(cancel_action, &QAction::triggered, this, &CalibrationWidget::ResetActions);

  // i18n: A button to finalize a game controller calibration process.
  auto* const finish_action = new QAction(tr("Finish Calibration"), this);
  connect(finish_action, &QAction::triggered, this, &CalibrationWidget::FinishCalibration);

  DeleteAllActions();

  addAction(finish_action);
  addAction(cancel_action);
  setDefaultAction(cancel_action);
}

void CalibrationWidget::FinishCalibration()
{
  const auto lock = m_mapping_widget.GetController()->GetStateLock();
  m_calibrator->ApplyResults(&m_input);
  ResetActions();
}

void CalibrationWidget::Update(Common::DVec2 point)
{
  // FYI: The "StateLock" is always held when this is called.

  QFont f = parentWidget()->font();
  QPalette p = parentWidget()->palette();

  if (IsMapping())
  {
    if (m_mapper->Update())
    {
      // Restart the animation for the next direction when progress is made.
      RestartAnimation();
    }

    if (m_mapper->IsComplete())
    {
      const bool needs_calibration = m_mapper->IsCalibrationNeeded();

      if (m_mapper->ApplyResults(m_mapping_widget.GetController(), &m_input))
      {
        emit m_mapping_widget.ConfigChanged();

        if (needs_calibration)
        {
          StartCalibration();
        }
        else
        {
          // Load square calibration for digital inputs.
          m_input.SetCalibrationFromGate(ControllerEmu::SquareStickGate{1});
          m_input.SetCenter({});

          ResetActions();
        }
      }
      else
      {
        ResetActions();
      }

      m_mapper.reset();
    }
  }
  else if (IsCalibrating())
  {
    m_calibrator->Update(point);

    if (!m_calibrator->IsCalibrationDataSensible())
    {
      m_stop_spinning_time = Clock::now() + STOP_SPINNING_DURATION;
    }
    else if (m_calibrator->IsComplete())
    {
      FinishCalibration();
    }
  }
  else if (IsPointOutsideCalibration(point, m_input))
  {
    // Bold and red on miscalibration.
    f.setBold(true);
    p.setColor(QPalette::ButtonText, Qt::red);
  }

  setFont(f);
  setPalette(p);
}

bool CalibrationWidget::IsActive() const
{
  return IsMapping() || IsCalibrating();
}

bool CalibrationWidget::IsMapping() const
{
  return m_mapper != nullptr;
}

bool CalibrationWidget::IsCalibrating() const
{
  return m_calibrator != nullptr;
}
