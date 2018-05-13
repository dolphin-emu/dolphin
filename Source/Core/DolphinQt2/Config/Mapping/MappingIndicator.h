// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

namespace ControllerEmu
{
class Control;
class ControlGroup;
class NumericSetting;
}

class QPaintEvent;
class QTimer;

class ControlReference;

class MappingIndicator : public QWidget
{
public:
  explicit MappingIndicator(ControllerEmu::ControlGroup* group);

private:
  void BindCursorControls(bool tilt);
  void BindStickControls();
  void BindMixedTriggersControls();

  void DrawCursor(bool tilt);
  void DrawStick();
  void DrawMixedTriggers();

  void paintEvent(QPaintEvent*) override;
  ControllerEmu::ControlGroup* m_group;

  // Stick settings
  ControlReference* m_stick_up;
  ControlReference* m_stick_down;
  ControlReference* m_stick_left;
  ControlReference* m_stick_right;
  ControlReference* m_stick_modifier;

  ControllerEmu::NumericSetting* m_stick_radius;
  ControllerEmu::NumericSetting* m_stick_deadzone;

  // Cursor settings
  ControlReference* m_cursor_up;
  ControlReference* m_cursor_down;
  ControlReference* m_cursor_left;
  ControlReference* m_cursor_right;
  ControlReference* m_cursor_forward;
  ControlReference* m_cursor_backward;

  ControllerEmu::NumericSetting* m_cursor_center;
  ControllerEmu::NumericSetting* m_cursor_width;
  ControllerEmu::NumericSetting* m_cursor_height;
  ControllerEmu::NumericSetting* m_cursor_deadzone;

  // Triggers settings
  ControlReference* m_mixed_triggers_r_analog;
  ControlReference* m_mixed_triggers_r_button;
  ControlReference* m_mixed_triggers_l_analog;
  ControlReference* m_mixed_triggers_l_button;

  ControllerEmu::NumericSetting* m_mixed_triggers_threshold;

  QTimer* m_timer;
};
