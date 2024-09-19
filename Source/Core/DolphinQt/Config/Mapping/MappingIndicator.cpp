// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/Mapping/MappingIndicator.h"

#include <array>
#include <cmath>

#include <fmt/format.h>

#include <QAction>
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include "Common/MathUtil.h"

#include "Core/HW/WiimoteEmu/Camera.h"

#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

#include "DolphinQt/Config/Mapping/MappingWidget.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

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
  QColor color = GetBBoxBrush().color().valueF() > 0.5 ? Qt::black : Qt::white;
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
  if (GetBBoxBrush().color().valueF() < 0.5)
    color->setHsvF(color->hueF(), color->saturationF(), 1 - color->valueF());
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

// Used to check if the user seems to have attempted proper calibration.
bool IsCalibrationDataSensible(const ControllerEmu::ReshapableInput::CalibrationData& data)
{
  // Test that the average input radius is not below a threshold.
  // This will make sure the user has actually moved their stick from neutral.

  // Even the GC controller's small range would pass this test.
  constexpr double REASONABLE_AVERAGE_RADIUS = 0.6;

  MathUtil::RunningVariance<ControlState> stats;

  for (auto& x : data)
    stats.Push(x);

  if (stats.Mean() < REASONABLE_AVERAGE_RADIUS)
  {
    return false;
  }

  // Test that the standard deviation is below a threshold.
  // This will make sure the user has not just filled in one side of their input.

  // Approx. deviation of a square input gate, anything much more than that would be unusual.
  constexpr double REASONABLE_DEVIATION = 0.14;

  return stats.StandardDeviation() < REASONABLE_DEVIATION;
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

}  // namespace

void MappingIndicator::paintEvent(QPaintEvent*)
{
  const auto lock = ControllerEmu::EmulatedController::GetStateLock();
  Draw();
}

void CursorIndicator::Draw()
{
  const auto adj_coord = m_cursor_group.GetState(true);

  DrawReshapableInput(m_cursor_group, CURSOR_TV_COLOR,
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

  p.translate(width() / 2, height() / 2);
  const auto scale = GetContentsScale();
  p.scale(scale, scale);
}

void ReshapableInputIndicator::DrawReshapableInput(
    ControllerEmu::ReshapableInput& stick, QColor gate_brush_color,
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
    DrawCalibration(p, raw_coord);
    return;
  }

  DrawUnderGate(p);

  QColor gate_pen_color = gate_brush_color.darker(125);

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

void AnalogStickIndicator::Draw()
{
  // Some hacks for pretty colors:
  const bool is_c_stick = m_group.name == "C-Stick";

  const auto gate_brush_color = is_c_stick ? C_STICK_GATE_COLOR : STICK_GATE_COLOR;

  const auto adj_coord = m_group.GetReshapableState(true);

  DrawReshapableInput(m_group, gate_brush_color,
                      (adj_coord.x || adj_coord.y) ? std::make_optional(adj_coord) : std::nullopt);
}

void TiltIndicator::Draw()
{
  WiimoteEmu::EmulateTilt(&m_motion_state, &m_group, 1.f / INDICATOR_UPDATE_FREQ);

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

  DrawReshapableInput(m_group, TILT_GATE_COLOR,
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

void SwingIndicator::Draw()
{
  auto& force = m_swing_group;
  WiimoteEmu::EmulateSwing(&m_motion_state, &force, 1.f / INDICATOR_UPDATE_FREQ);

  DrawReshapableInput(force, SWING_GATE_COLOR,
                      Common::DVec2{-m_motion_state.position.x, m_motion_state.position.z});
}

void ShakeMappingIndicator::Draw()
{
  constexpr std::size_t HISTORY_COUNT = INDICATOR_UPDATE_FREQ;

  WiimoteEmu::EmulateShake(&m_motion_state, &m_shake_group, 1.f / INDICATOR_UPDATE_FREQ);

  constexpr float MAX_DISTANCE = 0.5f;

  m_position_samples.push_front(m_motion_state.position / MAX_DISTANCE);
  // This also holds the current state so +1.
  if (m_position_samples.size() > HISTORY_COUNT + 1)
    m_position_samples.pop_back();

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

  // Grid line.
  if (m_grid_line_position ||
      std::any_of(m_position_samples.begin(), m_position_samples.end(),
                  [](const Common::Vec3& v) { return v.LengthSquared() != 0.0; }))
  {
    // Only start moving the line if there's non-zero data.
    m_grid_line_position = (m_grid_line_position + 1) % HISTORY_COUNT;
  }
  const double grid_line_x = 1.0 - m_grid_line_position * 2.0 / HISTORY_COUNT;
  p.setPen(QPen(GetRawInputColor(), 0));
  p.drawLine(QPointF{grid_line_x, -1.0}, QPointF{grid_line_x, 1.0});

  // Position history.
  const QColor component_colors[] = {Qt::blue, Qt::green, Qt::red};
  p.setBrush(Qt::NoBrush);
  for (std::size_t c = 0; c != raw_coord.data.size(); ++c)
  {
    QPolygonF polyline;

    int i = 0;
    for (auto& sample : m_position_samples)
    {
      polyline.append(QPointF{1.0 - i * 2.0 / HISTORY_COUNT, sample.data[c]});
      ++i;
    }

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
  p.translate(width() / 2, height() / 2);

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

void GyroMappingIndicator::Draw()
{
  const auto gyro_state = m_gyro_group.GetState();
  const auto raw_gyro_state = m_gyro_group.GetRawState();
  const auto angular_velocity = gyro_state.value_or(Common::Vec3{});
  const auto jitter = raw_gyro_state - m_previous_velocity;
  m_previous_velocity = raw_gyro_state;

  m_state *= WiimoteEmu::GetRotationFromGyroscope(angular_velocity * Common::Vec3(-1, +1, -1) /
                                                  INDICATOR_UPDATE_FREQ);
  m_state = m_state.Normalized();

  // Reset orientation when stable for a bit:
  constexpr u32 STABLE_RESET_STEPS = INDICATOR_UPDATE_FREQ;
  // Consider device stable when data (with deadzone applied) is zero.
  const bool is_stable = !angular_velocity.LengthSquared();

  if (!is_stable)
    m_stable_steps = 0;
  else if (m_stable_steps != STABLE_RESET_STEPS)
    ++m_stable_steps;

  if (STABLE_RESET_STEPS == m_stable_steps)
    m_state = Common::Quaternion::Identity();

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
  p.translate(width() / 2, height() / 2);

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

  auto pen = GetInputDotPen(m_ir_group.enabled ? GetAdjustedInputColor() : GetRawInputColor());

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

void ReshapableInputIndicator::DrawCalibration(QPainter& p, Common::DVec2 point)
{
  const auto center = m_calibration_widget->GetCenter();

  p.save();
  p.translate(center.x, center.y);

  // Input shape.
  p.setPen(GetInputShapePen());
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [this](double angle) { return m_calibration_widget->GetCalibrationRadiusAtAngle(angle); }));

  // Center.
  if (center.x || center.y)
  {
    p.setPen(GetInputDotPen(GetCenterColor()));
    p.drawPoint(QPointF{});
  }

  p.restore();

  // Stick position.
  p.setPen(GetInputDotPen(GetAdjustedInputColor()));
  p.drawPoint(QPointF{point.x, point.y});
}

void ReshapableInputIndicator::UpdateCalibrationWidget(Common::DVec2 point)
{
  if (m_calibration_widget)
    m_calibration_widget->Update(point);
}

bool ReshapableInputIndicator::IsCalibrating() const
{
  return m_calibration_widget && m_calibration_widget->IsCalibrating();
}

void ReshapableInputIndicator::SetCalibrationWidget(CalibrationWidget* widget)
{
  m_calibration_widget = widget;
}

CalibrationWidget::CalibrationWidget(ControllerEmu::ReshapableInput& input,
                                     ReshapableInputIndicator& indicator)
    : m_input(input), m_indicator(indicator), m_completion_action{}
{
  m_indicator.SetCalibrationWidget(this);

  // Make it more apparent that this is a menu with more options.
  setPopupMode(ToolButtonPopupMode::MenuButtonPopup);

  SetupActions();

  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

  m_informative_timer = new QTimer(this);
  connect(m_informative_timer, &QTimer::timeout, this, [this] {
    // If the user has started moving we'll assume they know what they are doing.
    if (*std::max_element(m_calibration_data.begin(), m_calibration_data.end()) > 0.5)
      return;

    ModalMessageBox::information(
        this, tr("Calibration"),
        tr("For best results please slowly move your input to all possible regions."));
  });
  m_informative_timer->setSingleShot(true);
}

void CalibrationWidget::SetupActions()
{
  const auto calibrate_action = new QAction(tr("Calibrate"), this);
  const auto center_action = new QAction(tr("Center and Calibrate"), this);
  const auto reset_action = new QAction(tr("Reset"), this);

  connect(calibrate_action, &QAction::triggered, [this]() {
    StartCalibration();
    m_new_center = Common::DVec2{};
  });
  connect(center_action, &QAction::triggered, [this]() {
    StartCalibration();
    m_new_center = std::nullopt;
  });
  connect(reset_action, &QAction::triggered, [this]() {
    m_input.SetCalibrationToDefault();
    m_input.SetCenter({0, 0});
  });

  for (auto* action : actions())
    removeAction(action);

  addAction(calibrate_action);
  addAction(center_action);
  addAction(reset_action);
  setDefaultAction(calibrate_action);

  m_completion_action = new QAction(tr("Finish Calibration"), this);
  connect(m_completion_action, &QAction::triggered, [this]() {
    m_input.SetCenter(GetCenter());
    m_input.SetCalibrationData(std::move(m_calibration_data));
    m_informative_timer->stop();
    SetupActions();
  });
}

void CalibrationWidget::StartCalibration()
{
  m_prev_point = {};
  m_calibration_data.assign(m_input.CALIBRATION_SAMPLE_COUNT, 0.0);

  // Cancel calibration.
  const auto cancel_action = new QAction(tr("Cancel Calibration"), this);
  connect(cancel_action, &QAction::triggered, [this]() {
    m_calibration_data.clear();
    m_informative_timer->stop();
    SetupActions();
  });

  for (auto* action : actions())
    removeAction(action);

  addAction(cancel_action);
  addAction(m_completion_action);
  setDefaultAction(cancel_action);

  // If the user doesn't seem to know what they are doing after a bit inform them.
  m_informative_timer->start(2000);
}

void CalibrationWidget::Update(Common::DVec2 point)
{
  QFont f = parentWidget()->font();
  QPalette p = parentWidget()->palette();

  // Use current point if center is being calibrated.
  if (!m_new_center.has_value())
    m_new_center = point;

  if (IsCalibrating())
  {
    const auto new_point = point - *m_new_center;
    m_input.UpdateCalibrationData(m_calibration_data, m_prev_point, new_point);
    m_prev_point = new_point;

    if (IsCalibrationDataSensible(m_calibration_data))
    {
      setDefaultAction(m_completion_action);
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

bool CalibrationWidget::IsCalibrating() const
{
  return !m_calibration_data.empty();
}

double CalibrationWidget::GetCalibrationRadiusAtAngle(double angle) const
{
  return m_input.GetCalibrationDataRadiusAtAngle(m_calibration_data, angle);
}

Common::DVec2 CalibrationWidget::GetCenter() const
{
  return m_new_center.value_or(Common::DVec2{});
}
