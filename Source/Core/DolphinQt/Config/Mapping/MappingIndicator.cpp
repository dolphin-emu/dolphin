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
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/Device.h"

#include "DolphinQt/Settings.h"

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
    BindMixedTriggersControls();
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

void MappingIndicator::BindMixedTriggersControls()
{
  m_mixed_triggers_l_button = m_group->controls[0]->control_ref.get();
  m_mixed_triggers_r_button = m_group->controls[1]->control_ref.get();
  m_mixed_triggers_l_analog = m_group->controls[2]->control_ref.get();
  m_mixed_triggers_r_analog = m_group->controls[3]->control_ref.get();

  m_mixed_triggers_threshold = m_group->numeric_settings[0].get();
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
  // 24 is a multiple of 8 (octagon) and enough points to be visibly pleasing:
  constexpr int shape_point_count = 24;
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
  const QColor gate_color = is_c_stick ? Qt::yellow : Qt::lightGray;

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
  p.setBrush(Qt::white);
  p.setPen(Qt::gray);
  p.drawRect(-scale - 1, -scale - 1, scale * 2 + 1, scale * 2 + 1);

  // UI y-axis is opposite that of stick.
  p.scale(1.0, -1.0);

  // Enable AA after drawing bounding box.
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  // Input gate. (i.e. the octagon shape)
  p.setPen(Qt::darkGray);
  p.setBrush(gate_color);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&stick](double ang) { return stick.GetGateRadiusAtAngle(ang); }, scale));

  // Deadzone.
  p.setPen(Qt::darkGray);
  p.setBrush(QBrush(Qt::darkGray, Qt::BDiagPattern));
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&stick](double ang) { return stick.GetDeadzoneRadiusAtAngle(ang); }, scale));

  // Input shape.
  p.setPen(QPen(Qt::darkGray, 1.0, Qt::DashLine));
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(GetPolygonFromRadiusGetter(
      [&stick](double ang) { return stick.GetInputRadiusAtAngle(ang); }, scale));

  // Raw stick position.
  p.setPen(Qt::NoPen);
  p.setBrush(Qt::darkGray);
  p.drawEllipse(QPointF{raw_coord.x, raw_coord.y} * scale, dot_radius, dot_radius);

  // Adjusted stick position.
  if (adj_coord.x || adj_coord.y)
  {
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::red);
    p.drawEllipse(QPointF{adj_coord.x, adj_coord.y} * scale, dot_radius, dot_radius);
  }
}

void MappingIndicator::DrawMixedTriggers()
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::TextAntialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  // Polled values
  double r_analog = PollControlState(m_mixed_triggers_r_analog);
  double r_button = PollControlState(m_mixed_triggers_r_button);
  double l_analog = PollControlState(m_mixed_triggers_l_analog);
  double l_button = PollControlState(m_mixed_triggers_l_button);
  double threshold = m_mixed_triggers_threshold->GetValue();

  double r_bar_percent = r_analog;
  double l_bar_percent = l_analog;

  if ((r_button && r_button != r_analog) || (r_button == r_analog && r_analog > threshold))
    r_bar_percent = 1;
  else
    r_bar_percent *= 0.8;

  if ((l_button && l_button != l_analog) || (l_button == l_analog && l_analog > threshold))
    l_bar_percent = 1;
  else
    l_bar_percent *= 0.8;

  p.fillRect(0, 0, width(), 64, Qt::black);

  p.fillRect(0, 0, l_bar_percent * width(), 32, Qt::red);
  p.fillRect(0, 32, r_bar_percent * width(), 32, Qt::red);

  p.setPen(Qt::white);
  p.drawLine(width() * 0.8, 0, width() * 0.8, 63);
  p.drawLine(0, 32, width(), 32);

  p.setPen(Qt::green);
  p.drawLine(width() * 0.8 * threshold, 0, width() * 0.8 * threshold, 63);

  p.setBrush(Qt::black);
  p.setPen(Qt::white);
  p.drawText(width() * 0.225, 20, tr("L-Analog"));
  p.drawText(width() * 0.8 + 16, 20, tr("L"));
  p.drawText(width() * 0.225, 52, tr("R-Analog"));
  p.drawText(width() * 0.8 + 16, 52, tr("R"));
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
