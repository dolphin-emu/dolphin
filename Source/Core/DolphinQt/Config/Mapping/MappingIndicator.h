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
}  // namespace ControllerEmu

class QPaintEvent;
class QTimer;

class ControlReference;

class MappingIndicator : public QWidget
{
public:
  explicit MappingIndicator(ControllerEmu::ControlGroup* group);

private:
  void BindCursorControls(bool tilt);

  void DrawCursor(bool tilt);
  void DrawStick();
  void DrawMixedTriggers();

  void paintEvent(QPaintEvent*) override;
  ControllerEmu::ControlGroup* m_group;

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

  QTimer* m_timer;
};
