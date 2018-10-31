// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Mapping/MappingIndicator.h"

#include <array>
#include <cmath>

#include <QPainter>
#include <QTimer>

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
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
    BindStickControls();
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

void MappingIndicator::BindStickControls()
{
  m_stick_up = m_group->controls[0]->control_ref.get();
  m_stick_down = m_group->controls[1]->control_ref.get();
  m_stick_left = m_group->controls[2]->control_ref.get();
  m_stick_right = m_group->controls[3]->control_ref.get();
  m_stick_modifier = m_group->controls[4]->control_ref.get();

  m_stick_radius = m_group->numeric_settings[0].get();
  m_stick_deadzone = m_group->numeric_settings[1].get();
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

void MappingIndicator::DrawStick()
{
  float centerx = width() / 2., centery = height() / 2.;

  bool c_stick = m_group->name == "C-Stick";
  bool classic_controller = m_group->name == "Left Stick" || m_group->name == "Right Stick";

  float ratio = 1;

  if (c_stick)
    ratio = 1.;
  else if (classic_controller)
    ratio = 0.9f;

  // Polled values
  float mod = PollControlState(m_stick_modifier) ? 0.5 : 1;
  float radius = m_stick_radius->GetValue();
  float curx = -PollControlState(m_stick_left) + PollControlState(m_stick_right),
        cury = -PollControlState(m_stick_up) + PollControlState(m_stick_down);
  // The maximum deadzone value covers 50% of the stick area
  float deadzone = m_stick_deadzone->GetValue() / 2.;

  // Size parameters
  float max_size = (height() / 2.5) / ratio;
  float stick_size = (height() / 3.) / ratio;

  // Emulated cursor position
  float virt_curx, virt_cury;

  if (std::abs(curx) < deadzone && std::abs(cury) < deadzone)
  {
    virt_curx = virt_cury = 0;
  }
  else
  {
    virt_curx = curx * mod;
    virt_cury = cury * mod;
  }

  // Coordinates for an octagon
  std::array<QPointF, 8> radius_octagon = {{
      QPointF(centerx, centery + stick_size),                                   // Bottom
      QPointF(centerx + stick_size / sqrt(2), centery + stick_size / sqrt(2)),  // Bottom Right
      QPointF(centerx + stick_size, centery),                                   // Right
      QPointF(centerx + stick_size / sqrt(2), centery - stick_size / sqrt(2)),  // Top Right
      QPointF(centerx, centery - stick_size),                                   // Top
      QPointF(centerx - stick_size / sqrt(2), centery - stick_size / sqrt(2)),  // Top Left
      QPointF(centerx - stick_size, centery),                                   // Left
      QPointF(centerx - stick_size / sqrt(2), centery + stick_size / sqrt(2))   // Bottom Left
  }};

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  // Draw maximum values
  p.setBrush(Qt::white);
  p.setPen(Qt::black);
  p.drawRect(centerx - max_size, centery - max_size, max_size * 2, max_size * 2);

  // Draw radius
  p.setBrush(c_stick ? Qt::yellow : Qt::darkGray);
  p.drawPolygon(radius_octagon.data(), static_cast<int>(radius_octagon.size()));

  // Draw deadzone
  p.setBrush(c_stick ? Qt::darkYellow : Qt::lightGray);
  p.drawEllipse(centerx - deadzone * stick_size, centery - deadzone * stick_size,
                deadzone * stick_size * 2, deadzone * stick_size * 2);

  // Draw stick
  p.setBrush(Qt::black);
  p.drawEllipse(centerx - 4 + curx * max_size, centery - 4 + cury * max_size, 8, 8);

  // Draw virtual stick
  p.setBrush(Qt::red);
  p.drawEllipse(centerx - 4 + virt_curx * max_size * radius,
                centery - 4 + virt_cury * max_size * radius, 8, 8);
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
